#ifndef COST_SATURATION_COST_PARTITIONING_GENERATOR_GREEDY_H
#define COST_SATURATION_COST_PARTITIONING_GENERATOR_GREEDY_H

#include "cost_partitioning_generator.h"

namespace cost_saturation {
class CostPartitioningGeneratorGreedy : public CostPartitioningGenerator {
    const bool use_random_initial_order;
    const bool reverse_initial_order;
    const bool use_stolen_costs;
    const bool use_negative_costs;
    const bool queue_zero_ratios;
    const bool dynamic;
    const bool pairwise;
    const bool steepest_ascent;
    const bool continue_after_switch;
    const bool switch_preferred_pairs;
    const double max_optimization_time;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;

    // Unpartitioned h values.
    std::vector<std::vector<int>> h_values_by_abstraction;
    std::vector<double> used_costs_by_abstraction;

    // Pairwise orderings.
    std::vector<std::vector<std::vector<int>>> pairwise_h_values;

    std::vector<int> random_order;

    int num_returned_orders;

protected:
    virtual void initialize(
        const TaskProxy &task_proxy,
        const std::vector<std::unique_ptr<Abstraction>> &abstractions,
        const std::vector<int> &costs);

public:
    explicit CostPartitioningGeneratorGreedy(const options::Options &opts);

    virtual CostPartitioning get_next_cost_partitioning(
        const TaskProxy &task_proxy,
        const std::vector<std::unique_ptr<Abstraction>> &abstractions,
        const std::vector<int> &costs,
        const State &state,
        CPFunction cp_function) override;
};
}

#endif
