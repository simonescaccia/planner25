#ifndef CEGAR_SHORTEST_PATHS_H
#define CEGAR_SHORTEST_PATHS_H

#include "transition.h"
#include "types.h"

#include <cassert>
#include <memory>
#include <queue>
#include <vector>

namespace cegar {
using Cost = uint64_t;

class HeapQueue {
    using Entry = std::pair<Cost, int>;

    struct compare_func {
        bool operator()(const Entry &lhs, const Entry &rhs) const {
            return lhs.first > rhs.first;
        }
    };

    class Heap
        : public std::priority_queue<Entry, std::vector<Entry>, compare_func> {
        // We inherit since our need access to the underlying container c which
        // is a protected member.
        friend class HeapQueue;
    };

    Heap heap;
public:
    void push(Cost key, int value) {
        heap.push(std::make_pair(key, value));
    }

    Entry pop() {
        assert(!heap.empty());
        Entry result = heap.top();
        heap.pop();
        return result;
    }

    bool empty() const {
        return heap.empty();
    }

    void clear() {
        heap.c.clear();
    }
};

class ShortestPaths {
    static const Cost DIRTY;
    static const Cost INF_COSTS;

    const bool debug;
    bool task_has_zero_costs;
    std::vector<Cost> operator_costs;

    // Keep data structures around to avoid reallocating them.
    HeapQueue candidate_queue;
    HeapQueue open_queue;
    std::vector<Cost> goal_distances;
    std::vector<int> dirty_states;
    using ShortestPathTree = std::vector<Transition>;
    ShortestPathTree shortest_path;

    int convert_to_32_bit_cost(Cost cost) const;
    Cost convert_to_64_bit_cost(int cost) const;

    Cost get_h_value(int state_id) const;
    void set_h_value(int state_id, Cost h);
    void mark_dirty(int state);
    void mark_orphaned_predecessors(
        const std::vector<Transitions> &in, int state);

public:
    explicit ShortestPaths(const std::vector<int> &costs, bool debug);

    std::unique_ptr<Solution> extract_solution_from_shortest_path_tree(
        int init_id,
        const Goals &goals);

    void dijkstra_from_orphans(
        const std::vector<Transitions> &in,
        const std::vector<Transitions> &out,
        int v, int v1, int v2, bool filter_orphans);
    void full_dijkstra(
        const std::vector<Transitions> &transitions,
        const std::unordered_set<int> &goals);
    bool test_distances(
        const std::vector<Transitions> &in,
        const std::vector<Transitions> &out,
        const std::unordered_set<int> &goals);
};
}

#endif
