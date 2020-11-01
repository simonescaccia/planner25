#include "pho_heuristic.h"

#include "abstraction.h"
#include "cost_partitioning_heuristic_collection_generator.h"
#include "cost_partitioning_heuristic.h"
#include "max_cost_partitioning_heuristic.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"

#include <cmath>

using namespace std;

namespace cost_saturation {
PhO::PhO(
    const Abstractions &abstractions,
    const vector<int> &costs,
    lp::LPSolverType solver_type,
    bool saturated,
    bool debug)
    : lp_solver(solver_type),
      debug(debug) {
    double infinity = lp_solver.get_infinity();
    int num_abstractions = abstractions.size();
    int num_operators = costs.size();

    saturated_costs_by_abstraction.reserve(num_abstractions);
    h_values_by_abstraction.reserve(num_abstractions);
    for (int i = 0; i < num_abstractions; ++i) {
        const Abstraction &abstraction = *abstractions[i];
        vector<int> h_values = abstraction.compute_goal_distances(costs);
        vector<int> saturated_costs = abstraction.compute_saturated_costs(h_values);
        h_values_by_abstraction.push_back(move(h_values));
        saturated_costs_by_abstraction.push_back(move(saturated_costs));
    }

    vector<lp::LPVariable> variables;
    variables.reserve(num_abstractions);
    for (int i = 0; i < num_abstractions; ++i) {
        // Objective coefficients are set below.
        variables.emplace_back(0, infinity, 0);
    }

    vector<lp::LPConstraint> constraints;
    constraints.reserve(num_operators);
    for (int op_id = 0; op_id < num_operators; ++op_id) {
        lp::LPConstraint constraint(-infinity, costs[op_id]);
        for (int i = 0; i < num_abstractions; ++i) {
            if (saturated) {
                int scf_h = saturated_costs_by_abstraction[i][op_id];
                if (scf_h == -INF) {
                    // The constraint is always satisfied and we can ignore it.
                    continue;
                }
                if (scf_h != 0) {
                    constraint.insert(i, scf_h);
                }
            } else if (abstractions[i]->operator_is_active(op_id) && costs[op_id] != 0) {
                constraint.insert(i, costs[op_id]);
            }
        }
        if (!constraint.empty()) {
            constraints.push_back(move(constraint));
        }
    }

    lp_solver.load_problem(lp::LPObjectiveSense::MAXIMIZE, variables, constraints);
}

CostPartitioningHeuristic PhO::compute_cost_partitioning(
    const Abstractions &abstractions,
    const vector<int> &,
    const vector<int> &costs,
    const vector<int> &abstract_state_ids) {
    int num_abstractions = abstractions.size();
    int num_operators = costs.size();

    for (int i = 0; i < num_abstractions; ++i) {
        int h = h_values_by_abstraction[i][abstract_state_ids[i]];
        lp_solver.set_objective_coefficient(i, h);
    }
    lp_solver.solve();

    if (!lp_solver.has_optimal_solution()) {
        // State is unsolvable.
        vector<int> zero_costs(num_operators, 0);
        CostPartitioningHeuristic cp_heuristic;
        for (int i = 0; i < num_abstractions; ++i) {
            vector<int> h_values = abstractions[i]->compute_goal_distances(zero_costs);
            cp_heuristic.add_h_values(i, move(h_values));
        }
        return cp_heuristic;
    }

    double epsilon = 0.01;
    double objective_value = lp_solver.get_objective_value();
    int result = ceil(objective_value - epsilon);
    if (debug) {
        cout << "Objective value: " << objective_value << " -> " << result << endl;
    }
    vector<double> solution = lp_solver.extract_solution();
    CostPartitioningHeuristic cp_heuristic;
    for (int i = 0; i < num_abstractions; ++i) {
        vector<int> weighted_costs;
        weighted_costs.reserve(num_operators);
        for (int op_id = 0; op_id < num_operators; ++op_id) {
            int cost = solution[i] * saturated_costs_by_abstraction[i][op_id];
            weighted_costs.push_back(cost);
            if (debug && false) {
                cout << "Weighted cost: " << solution[i] << " * " << costs[op_id]
                     << " = " << solution[i] * costs[op_id] << " -> " << cost << endl;
            }
        }
        vector<int> h_values = abstractions[i]->compute_goal_distances(weighted_costs);
        cp_heuristic.add_h_values(i, move(h_values));
    }
    if (debug) {
        cout << "CP value: " << cp_heuristic.compute_heuristic(abstract_state_ids) << endl;
    }
    return cp_heuristic;
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Post-hoc optimization heuristic",
        "Compute the maximum over multiple PhO heuristics.");

    prepare_parser_for_cost_partitioning_heuristic(parser);
    parser.add_option<bool>("saturated", "saturate costs", "true");
    add_order_options_to_parser(parser);
    lp::add_lp_solver_option_to_parser(parser);
    utils::add_verbosity_option_to_parser(parser);

    options::Options opts = parser.parse();
    if (parser.help_mode() || parser.dry_run()) {
        return nullptr;
    }

    shared_ptr<AbstractTask> task = get_scaled_costs_task(
        opts.get<shared_ptr<AbstractTask>>("transform"), COST_FACTOR);
    opts.set<shared_ptr<AbstractTask>>("transform", task);

    TaskProxy task_proxy(*task);
    vector<int> costs = task_properties::get_operator_costs(task_proxy);
    Abstractions abstractions = generate_abstractions(
        task, opts.get_list<shared_ptr<AbstractionGenerator>>("abstractions"));
    PhO pho(abstractions, costs, opts.get<lp::LPSolverType>("lpsolver"),
            opts.get<bool>("saturated"),
            opts.get<utils::Verbosity>("verbosity") == utils::Verbosity::DEBUG);
    CPFunction cp_function = [&pho](const Abstractions &abstractions,
                                    const vector<int> &order,
                                    const vector<int> &costs,
                                    const vector<int> &abstract_state_ids) {
            return pho.compute_cost_partitioning(abstractions, order, costs, abstract_state_ids);
        };
    vector<CostPartitioningHeuristic> cp_heuristics =
        get_cp_heuristic_collection_generator_from_options(opts).generate_cost_partitionings(
            task_proxy, abstractions, costs, cp_function);
    return make_shared<MaxCostPartitioningHeuristic>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("pho", _parse);
}
