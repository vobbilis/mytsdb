
// Helper for map key in aggregation
struct LabelSetComparator {
    bool operator()(const LabelSet& a, const LabelSet& b) const {
        return a.labels() < b.labels();
    }
};
