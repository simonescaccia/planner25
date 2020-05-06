#include "utils.h"

#include "abstract_state.h"
#include "abstraction.h"
#include "transition.h"
#include "transition_system.h"

#include "../option_parser.h"

#include "../heuristics/additive_heuristic.h"
#include "../task_utils/task_properties.h"
#include "../utils/memory.h"

#include <algorithm>
#include <cassert>
#include <map>

using namespace std;

namespace cegar {
unique_ptr<additive_heuristic::AdditiveHeuristic> create_additive_heuristic(
    const shared_ptr<AbstractTask> &task) {
    Options opts;
    opts.set<shared_ptr<AbstractTask>>("transform", task);
    opts.set<bool>("cache_estimates", false);
    return utils::make_unique_ptr<additive_heuristic::AdditiveHeuristic>(opts);
}

static bool operator_applicable(
    const OperatorProxy &op, const utils::HashSet<FactProxy> &facts) {
    for (FactProxy precondition : op.get_preconditions()) {
        if (facts.count(precondition) == 0)
            return false;
    }
    return true;
}

static bool operator_achieves_fact(
    const OperatorProxy &op, const FactProxy &fact) {
    for (EffectProxy effect : op.get_effects()) {
        if (effect.get_fact() == fact)
            return true;
    }
    return false;
}

static utils::HashSet<FactProxy> compute_possibly_before_facts(
    const TaskProxy &task, const FactProxy &last_fact) {
    utils::HashSet<FactProxy> pb_facts;

    // Add facts from initial state.
    for (FactProxy fact : task.get_initial_state())
        pb_facts.insert(fact);

    // Until no more facts can be added:
    size_t last_num_reached = 0;
    /*
      Note: This can be done more efficiently by maintaining the number
      of unsatisfied preconditions for each operator and a queue of
      unhandled effects.

      TODO: Find out if this code is time critical, and change it if it
      is.
    */
    while (last_num_reached != pb_facts.size()) {
        last_num_reached = pb_facts.size();
        for (OperatorProxy op : task.get_operators()) {
            // Ignore operators that achieve last_fact.
            if (operator_achieves_fact(op, last_fact))
                continue;
            // Add all facts that are achieved by an applicable operator.
            if (operator_applicable(op, pb_facts)) {
                for (EffectProxy effect : op.get_effects()) {
                    pb_facts.insert(effect.get_fact());
                }
            }
        }
    }
    return pb_facts;
}

utils::HashSet<FactProxy> get_relaxed_possible_before(
    const TaskProxy &task, const FactProxy &fact) {
    utils::HashSet<FactProxy> reachable_facts =
        compute_possibly_before_facts(task, fact);
    reachable_facts.insert(fact);
    return reachable_facts;
}

vector<int> get_domain_sizes(const TaskProxy &task) {
    vector<int> domain_sizes;
    for (VariableProxy var : task.get_variables())
        domain_sizes.push_back(var.get_domain_size());
    return domain_sizes;
}

void add_h_update_option(options::OptionParser &parser) {
    vector<string> h_update;
    h_update.push_back("STATES_ON_TRACE");
    h_update.push_back("COST_MINUS_G");
    h_update.push_back("FULL_DIJKSTRA");
    h_update.push_back("DIJKSTRA_FROM_ORPHANS");
    h_update.push_back("DIJKSTRA_FROM_UNCONNECTED_ORPHANS");
    h_update.push_back("INCREMENTAL_UNINFORMED_SEARCH");
    h_update.push_back("INCREMENTAL_HEURISTIC_SEARCH");
    h_update.push_back("OPTIMIZED_INCREMENTAL_UNINFORMED_SEARCH");
    h_update.push_back("OPTIMIZED_INCREMENTAL_HEURISTIC_SEARCH");
    parser.add_enum_option(
        "h_update",
        h_update,
        "strategy for updating goal distances or distance estimates",
        "COST_MINUS_G");
}

void dump_dot_graph(const Abstraction &abstraction) {
    int num_states = abstraction.get_num_states();
    cout << "digraph transition_system";
    cout << " {" << endl;
    cout << "    node [shape = none] start;" << endl;
    for (int i = 0; i < num_states; ++i) {
        bool is_init = (i == abstraction.get_initial_state().get_id());
        bool is_goal = abstraction.get_goals().count(i);
        cout << "    node [shape = " << (is_goal ? "doublecircle" : "circle")
             << "] " << i << ";" << endl;
        if (is_init)
            cout << "    start -> " << i << ";" << endl;
    }
    for (int state_id = 0; state_id < num_states; ++state_id) {
        map<int, vector<int>> parallel_transitions;
        auto transitions = abstraction.get_transition_system().get_outgoing_transitions();
        for (const Transition &t : transitions[state_id]) {
            parallel_transitions[t.target_id].push_back(t.op_id);
        }
        for (auto &pair : parallel_transitions) {
            int target = pair.first;
            vector<int> &operators = pair.second;
            sort(operators.begin(), operators.end());
            cout << "    " << state_id << " -> " << target
                 << " [label = \"" << utils::join(operators, "_") << "\"];" << endl;
        }
    }
    cout << "}" << endl;
}
}
