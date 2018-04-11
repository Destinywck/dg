#include "analysis/ReachingDefinitions/Srg/MarkerSRGBuilderFS.h"

using namespace dg::analysis::rd::srg;
/**
 * Saves the current definition of certain variable in given block
 * Used from value numbering procedures.
 */
void MarkerSRGBuilderFS::writeVariableStrong(const DefSite& var, NodeT *assignment, BlockT *block) {
    detail::Interval interval = concretize(detail::Interval{var.offset, var.len});
    current_weak_def[var.target][block].killOverlapping(interval);
    // remember the last definition
    current_def[var.target][block].add(std::move(interval), assignment);
}

void MarkerSRGBuilderFS::writeVariableWeak(const DefSite& var, NodeT *assignment, BlockT *block) {
    current_weak_def[var.target][block].add(concretize(detail::Interval{var.offset, var.len}), assignment);
}

std::vector<MarkerSRGBuilderFS::NodeT *> MarkerSRGBuilderFS::readVariable(const DefSite& var, BlockT *read, const Intervals& covered) {
    assert( read );

    auto& block_defs = current_def[var.target];
    auto it = block_defs.find(read);
    std::vector<NodeT *> result;
    const auto interval = concretize(detail::Interval{var.offset, var.len});

    // find the last definition
    if (it != block_defs.end()) {
        Intervals cov;
        bool is_covered = false;
        std::tie(result, cov, is_covered) = it->second.collect(interval, covered);
        if (!is_covered || interval.isUnknown()) {
            NodeT *phi = readVariableRecursive(var, read, cov);
            result.push_back(phi);
        }
    } else {
        result.push_back(readVariableRecursive(var, read, covered));
    }

    // add weak defs
    auto block_weak_defs = current_weak_def[var.target][read].collectAll(interval);
    std::move(block_weak_defs.begin(), block_weak_defs.end(), std::back_inserter(result));

    return result;
}

void MarkerSRGBuilderFS::addPhiOperands(DefSite var, NodeT *phi, BlockT *block, const std::vector<detail::Interval>& covered) {

    if (var.len == 0 || var.offset.isUnknown()) {
        var.len = Offset::UNKNOWN;
        var.offset = 0;
    }
    phi->addDef(var, true);
    phi->addUse(var);

    for (BlockT *pred : block->predecessors()) {
        std::vector<NodeT *> assignments;
        Intervals cov;
        bool is_covered = false;
        const auto interval = detail::Interval{var.offset, var.len};
        std::tie(assignments,cov,is_covered) = last_def[var.target][pred].collect(interval, covered);
        if (!is_covered) {
            std::vector<NodeT *> assignments2 = readVariable(var, pred, cov);
            assignments.insert(assignments.begin(), assignments2.begin(), assignments2.end());
        }

        // add weak updates
        auto weak_defs = last_weak_def[var.target][pred].collectAll(interval);
        std::move(weak_defs.begin(), weak_defs.end(), std::back_inserter(assignments));

        for (auto& assignment : assignments)
            insertSrgEdge(assignment, phi, var);
    }
}

MarkerSRGBuilderFS::NodeT *MarkerSRGBuilderFS::readVariableRecursive(const DefSite& var, BlockT *block, const std::vector<detail::Interval>& covered) {
    std::vector<NodeT *> result;

    const auto interval = concretize(detail::Interval{var.offset, var.len});
    auto phi = std::unique_ptr<NodeT>(new NodeT(RDNodeType::PHI));

    phi->setBasicBlock(block);
    // writeVariableStrong kills current weak definitions, which are needed in the phi node, so we need to lookup them first.
    auto weak_defs = current_weak_def[var.target][block].collectAll(interval);
    for (auto& assignment : weak_defs)
        insertSrgEdge(assignment, phi.get(), var);

    writeVariableStrong(var, phi.get(), block);
    addPhiOperands(var, phi.get(), block, covered);

    NodeT *val = phi.get();
    phi_nodes.push_back(std::move(phi));
    return val;
}
