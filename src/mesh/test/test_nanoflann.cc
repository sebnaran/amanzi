#include <ctime>
#include <cstdlib>
#include <iostream>

#include "UnitTest++.h"
#include "nanoflann.hpp"

#include "KDTree.hh"
#include "Point.hh"

typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Adaptor<double, Amanzi::AmanziMesh::PointCloud>,
    Amanzi::AmanziMesh::PointCloud, -1> MyKDTree;

TEST(NANOFLANN) {
  // generate points
  int d(2), n(10);
  double range(1.0);
  double query_pt[2] = {0.5, 0.5};
  Amanzi::AmanziGeometry::Point p(0.5, 0.5);

  std::vector<Amanzi::AmanziGeometry::Point> points;

  for (int i = 0; i < n; ++i) {
    double x = range * (rand() % 1000) / 1000.0;
    double y = range * (rand() % 1000) / 1000.0;
    points.push_back(Amanzi::AmanziGeometry::Point(x, y));
  }

  std::cout << "Input points:\n";
  for (int i = 0; i < n; i++) {
    double dist = Amanzi::AmanziGeometry::norm(points[i] - p);
    std::cout << " " << i << " " << points[i] << " dist=" << dist << std::endl;
  }

  // construct a kd-tree index:
  Amanzi::AmanziMesh::PointCloud cloud;
  cloud.Init(&points);

  MyKDTree tree(d, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  tree.buildIndex();

  // SEARCH 1: closest points
  int nresults = 2;
  std::vector<size_t> idx(nresults);
  std::vector<double> dist_sqr(nresults);

  nresults = tree.knnSearch(&query_pt[0], nresults, &idx[0], &dist_sqr[0]);
		
  // In case of less points in the tree than requested:
  idx.resize(nresults);
  dist_sqr.resize(nresults);

  std::cout << "\nSearch results: found " << nresults << " points.\n";
  for (int i = 0; i < nresults; i++) {
    int n = idx[i];
    double dist = std::pow(dist_sqr[i], 0.5);
    std::cout << " point: " << n << " dist=" << dist << " xy=" << points[n] << std::endl;
    CHECK_CLOSE(dist, Amanzi::AmanziGeometry::norm(points[n] - p), 1e-14);
  }

  // SEARCH 2: points is a ball 
  double radius = 0.14;
  std::vector<std::pair<size_t, double> > matches;

  nanoflann::SearchParams params;
  // params.sorted = false;

  nresults = tree.radiusSearch(&query_pt[0], radius, matches, params);

  std::cout << "\nSearch results: radius=" << std::pow(radius, 0.5) << " -> " << nresults << " points inside\n";
  for (int i = 0; i < nresults; ++i) {
    int n = matches[i].first;
    double dist = std::pow(matches[i].second, 0.5);
    std::cout << " point: " << n << " dist=" << dist << " xy=" << points[n] << std::endl;
    CHECK_CLOSE(dist, Amanzi::AmanziGeometry::norm(points[n] - p), 1e-14);
  }
}
