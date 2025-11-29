// Copyright © 2024 Mel Henning
// SPDX-License-Identifier: MIT

use std::collections::HashMap;
use std::hash::Hash;

#[derive(Copy, Clone)]
struct Root<X: Copy> {
    size: usize,
    representative: X,
}

#[derive(Copy, Clone)]
enum Node<X: Copy> {
    Child { parent_idx: usize },
    Root(Root<X>),
}

/// Union-find structure
///
/// This implementation follows Tarjan and van Leeuwen - specifically the
/// "link by size" and "halving" variant.
///
/// Robert E. Tarjan and Jan van Leeuwen. 1984. Worst-case Analysis of Set
///     Union Algorithms. J. ACM 31, 2 (April 1984), 245–281.
///     https://doi.org/10.1145/62.2160
pub struct UnionFind<X: Copy + Hash + Eq> {
    idx_map: HashMap<X, usize>,
    nodes: Vec<Node<X>>,
}

impl<X: Copy + Hash + Eq> UnionFind<X> {
    /// Create a new union-find structure
    ///
    /// At initialization, each possible value is in its own set
    pub fn new() -> Self {
        UnionFind {
            idx_map: HashMap::new(),
            nodes: Vec::new(),
        }
    }

    fn find_root(&mut self, mut idx: usize) -> (usize, Root<X>) {
        loop {
            match self.nodes[idx] {
                Node::Child { parent_idx } => {
                    match self.nodes[parent_idx] {
                        Node::Child {
                            parent_idx: grandparent_idx,
                        } => {
                            // "Halving" in Tarjan and van Leeuwen
                            self.nodes[idx] = Node::Child {
                                parent_idx: grandparent_idx,
                            };
                            idx = grandparent_idx;
                        }
                        Node::Root(parent_root) => {
                            return (parent_idx, parent_root)
                        }
                    }
                }
                Node::Root(root) => return (idx, root),
            }
        }
    }

    /// Find the representative element for x
    pub fn find(&mut self, x: X) -> X {
        match self.idx_map.get(&x) {
            Some(&idx) => {
                let (_, Root { representative, .. }) = self.find_root(idx);
                representative
            }
            None => x,
        }
    }

    fn map_or_create(&mut self, x: X) -> usize {
        *self.idx_map.entry(x).or_insert_with(|| {
            self.nodes.push(Node::Root(Root {
                size: 1,
                representative: x,
            }));
            self.nodes.len() - 1
        })
    }

    /// Union the sets containing a and b
    ///
    /// The representative for a will become the representative of
    /// the combined set
    pub fn union(&mut self, a: X, b: X) {
        if a == b {
            return;
        }

        let a_idx = self.map_or_create(a);
        let b_idx = self.map_or_create(b);
        let (a_root_idx, a_root) = self.find_root(a_idx);
        let (b_root_idx, b_root) = self.find_root(b_idx);

        if a_root_idx != b_root_idx {
            // Keep the tree balanced
            let (new_root_idx, new_child_idx) = if a_root.size >= b_root.size {
                (a_root_idx, b_root_idx)
            } else {
                (b_root_idx, a_root_idx)
            };

            self.nodes[new_root_idx] = Node::Root(Root {
                size: a_root.size + b_root.size,
                representative: a_root.representative,
            });
            self.nodes[new_child_idx] = Node::Child {
                parent_idx: new_root_idx,
            };
        }
    }

    /// Return true if find() is the identity mapping
    pub fn is_empty(&self) -> bool {
        self.nodes.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use crate::union_find::Node;
    use crate::union_find::UnionFind;
    use std::cmp::max;
    use std::hash::Hash;

    fn ceil_log2(x: usize) -> u32 {
        assert!(x > 0);
        usize::BITS - (x - 1).leading_zeros()
    }

    struct HeightInfo {
        height: u32,
        size: usize,
    }

    pub struct HeightCalc<'a, X: Copy + Hash + Eq> {
        uf: &'a UnionFind<X>,
        downward_edges: Vec<Vec<usize>>,
    }

    impl<'a, X: Copy + Hash + Eq> HeightCalc<'a, X> {
        fn new(uf: &'a UnionFind<X>) -> Self {
            let mut downward_edges: Vec<Vec<usize>> =
                uf.nodes.iter().map(|_| Vec::new()).collect();
            for (i, node) in uf.nodes.iter().enumerate() {
                if let Node::Child { parent_idx } = node {
                    downward_edges[*parent_idx].push(i);
                }
            }

            HeightCalc { uf, downward_edges }
        }

        fn calc_info(&self, idx: usize) -> HeightInfo {
            let mut result = HeightInfo { height: 0, size: 1 };
            for child in &self.downward_edges[idx] {
                let child_result = self.calc_info(*child);
                result.height = max(result.height, child_result.height + 1);
                result.size += child_result.size;
            }
            result
        }

        fn check_roots(&self) -> u32 {
            let mut total_size = 0;
            let mut max_height = 0;
            for (i, node) in self.uf.nodes.iter().enumerate() {
                if let Node::Root(root) = node {
                    let info = self.calc_info(i);
                    assert_eq!(root.size, info.size);

                    total_size += info.size;
                    max_height = max(max_height, info.height);

                    let max_expected_height = ceil_log2(root.size + 1) - 1;
                    if info.height > max_expected_height {
                        eprintln!(
                            "height {}\t max_expected_height {}\t size {}",
                            info.height, max_expected_height, info.size
                        );
                    }
                    assert!(info.height <= max_expected_height);
                }
            }
            assert_eq!(total_size, self.uf.nodes.len());
            assert_eq!(total_size, self.uf.idx_map.len());
            return max_height;
        }

        pub fn check(uf: &'a UnionFind<X>) -> u32 {
            HeightCalc::new(uf).check_roots()
        }
    }

    #[test]
    fn test_basic() {
        let mut f = UnionFind::new();
        assert_eq!(f.find(10), 10);
        assert_eq!(f.find(12), 12);

        f.union(10, 12);
        f.union(11, 13);

        HeightCalc::check(&f);

        assert_eq!(f.find(13), 11);
        assert_eq!(f.find(12), 10);
        assert_eq!(f.find(11), 11);
        assert_eq!(f.find(10), 10);

        f.union(12, 13);

        HeightCalc::check(&f);

        assert_eq!(f.find(13), 10);
        assert_eq!(f.find(12), 10);
        assert_eq!(f.find(11), 10);
        assert_eq!(f.find(10), 10);

        assert_eq!(f.find(14), 14);

        HeightCalc::check(&f);

        // Union the set with itself
        f.union(11, 10);

        HeightCalc::check(&f);

        assert_eq!(f.find(13), 10);
        assert_eq!(f.find(12), 10);
        assert_eq!(f.find(11), 10);
        assert_eq!(f.find(10), 10);
    }

    #[test]
    fn test_chain_a_height() {
        let mut f = UnionFind::new();
        for i in 0..1000 {
            f.union(i, i + 1);
            HeightCalc::check(&f);
        }
        assert_eq!(f.find(1000), 0);
    }

    #[test]
    fn test_chain_b_height() {
        let mut f = UnionFind::new();
        for i in 0..1000 {
            f.union(i + 1, i);
            HeightCalc::check(&f);
        }
        assert_eq!(f.find(0), 1000);
    }

    #[test]
    fn test_binary_tree_height() {
        let height = 8;
        let count = 1 << height;

        let mut f = UnionFind::new();
        for current_height in 0..height {
            let stride = 1 << current_height;
            for i in (0..count).step_by(2 * stride) {
                f.union(i, i + stride);
            }
            let actual_height = HeightCalc::check(&f);

            // actual_height can vary based on tiebreaker condition
            assert!(
                actual_height == current_height
                    || actual_height == current_height + 1
            );
        }

        // Check path halving
        let actual_height_before = HeightCalc::check(&f);
        for i in 0..count {
            assert_eq!(f.find(i), 0);
        }
        let actual_height_after = HeightCalc::check(&f);

        assert!(actual_height_after <= actual_height_before.div_ceil(2));
    }
}
