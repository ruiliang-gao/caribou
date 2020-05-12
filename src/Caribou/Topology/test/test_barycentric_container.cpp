#include <gtest/gtest.h>
#include "topology_test.h"
#include <Caribou/Geometry/Quad.h>
#include <Caribou/Topology/Mesh.h>
#include <Caribou/Topology/Domain.h>
#include <Caribou/Topology/BarycentricContainer.h>

using namespace caribou::topology;
using namespace caribou::geometry;
using namespace caribou;

TEST(BarycentricContainer, _2D) {
    using Mesh = Mesh<_2D>;
    using Quad = Quad<_2D, Linear>;
    // Let's create a container mesh consisting of quads
    //
    // 6:(-5, 5)              7:(0, 5)       8:(5, 5)
    //             +-------------+--------------+
    //             |             |              |
    //             |             |              |
    //             |      2      |      3       |
    //             |             |              |
    //             |             | 4:(0, 0)     |
    // 3:(-5, 0)   +-------------+--------------+ 5:(5, 0)
    //             |             |              |
    //             |             |              |
    //             |      0      |      1       |
    //             |             |              |
    //             |             |              |
    //             +-------------+--------------+
    //         0:(-5, -5)     1:(0, -5)      2:(5, -5)
    std::vector<Mesh::WorldCoordinates> positions = {
        {-5, -5},
        { 0, -5},
        { 5, -5},
        {-5,  0},
        { 0,  0},
        { 5,  0},
        {-5,  5},
        { 0,  5},
        { 5,  5}
    };
    Mesh container_mesh (positions);

    Domain<Quad>::ElementsIndices quad_indices(4, 4);
    quad_indices << 0, 1, 4, 3,
                    1, 2, 5, 4,
                    3, 4, 7, 6,
                    4, 5, 8, 7;
    const Domain<Quad> * container_domain = container_mesh.add_domain<Quad>("quads", quad_indices);

    // Create the barycentric container
    BarycentricContainer barycentric_container(container_domain);

    // Make sure we are able to find all nodes of the container mesh
    for (const auto & p : positions) {
        const auto res = barycentric_container.barycentric_point(p);
        const Quad element = container_domain->element(res.element_index);
        EXPECT_MATRIX_NEAR(p, element.world_coordinates(res.local_coordinates), 1e-10);
    }

    // Make sure we are able to find all gauss points and center positions of the container elements
    for (UNSIGNED_INTEGER_TYPE element_id = 0; element_id < container_domain->number_of_elements(); ++element_id) {
        const Quad element = container_domain->element(element_id);
        // Gauss points
        for (const auto & gauss_node : element.gauss_nodes()) {
            const auto res = barycentric_container.barycentric_point(element.world_coordinates(gauss_node.position));
            EXPECT_EQ(res.element_index, static_cast<INTEGER_TYPE>(element_id));
            EXPECT_MATRIX_NEAR(res.local_coordinates, gauss_node.position, 1e-10);
        }

        // Center position
        const auto res = barycentric_container.barycentric_point(element.center());
        EXPECT_EQ(res.element_index, static_cast<INTEGER_TYPE>(element_id));
        EXPECT_MATRIX_NEAR(element.world_coordinates(res.local_coordinates), element.center(), 1e-10);
    }

    // Embedded mesh 1

    //          ----------+----------
    //          |         |         |
    //          |         |         |
    //          |    +----+----+    |
    //          |    |  2 |  3 |    |
    //          |    |    |    |    |
    //          |----+----+----+----|
    //          |    |  0 |  1 |    |
    //          |    |    |    |    |
    //          |    +----+----+    |
    //          |         |         |
    //          |         |         |
    //          ---------------------

    std::vector<Mesh::WorldCoordinates> embedded_positions_1 = {
        {-2.5, -2.5},
        {   0, -2.5},
        { 2.5, -2.5},
        {-2.5,    0},
        {   0,    0},
        { 2.5,    0},
        {-2.5,  2.5},
        {   0,  2.5},
        { 2.5,  2.5}
    };

    Mesh embedded_mesh_1 (embedded_positions_1);

    Domain<Quad>::ElementsIndices embedded_quad_indices_1(4, 4);
    embedded_quad_indices_1 << 0, 1, 4, 3,
                               1, 2, 5, 4,
                               3, 4, 7, 6,
                               4, 5, 8, 7;
    embedded_mesh_1.add_domain<Quad>("quads", embedded_quad_indices_1);

    barycentric_container.add_embedded_mesh(&embedded_mesh_1);

    // Make sure we are able to find all nodes of the embedded mesh
    for (UNSIGNED_INTEGER_TYPE node_id = 0; node_id < embedded_mesh_1.number_of_nodes(); ++node_id) {
        const auto bary_p = barycentric_container.barycentric_point(embedded_mesh_1, node_id);
        const Quad element = container_domain->element(bary_p.element_index);
        EXPECT_MATRIX_NEAR(embedded_mesh_1.position(node_id), element.world_coordinates(bary_p.local_coordinates), 1e-10);
    }

    // Interpolation
    Eigen::Map<Eigen::Matrix<FLOATING_POINT_TYPE, Eigen::Dynamic, 2, Eigen::RowMajor>> values(&positions[0][0], positions.size(), 2);
    Eigen::Matrix<FLOATING_POINT_TYPE, 9, 2> interpolated_values;
    barycentric_container.interpolate_field(embedded_mesh_1, values, interpolated_values);
    for (UNSIGNED_INTEGER_TYPE node_id = 0; node_id < (unsigned) interpolated_values.rows(); ++node_id) {
        EXPECT_MATRIX_EQUAL(interpolated_values.row(node_id).transpose(), embedded_positions_1[node_id]);
    }

    // Embedded mesh 2

    //      +----+----+
    //      |    |    |  <--- outside region
    //      |    |    |
    //      +----+----+---------------
    //      |    |    |    |         |
    //      |    |    |    |         |
    //      +----+----+    |         |
    //        ^  |         |         |
    //        |  |         |         |
    //        |  |-------------------|
    //   outside |         |         |
    //   region  |         |         |
    //           |         |         |
    //           |         |         |
    //           |         |         |
    //           ---------------------

    std::vector<Mesh::WorldCoordinates> embedded_positions_2 = {
        {-7.5,  2.5},
        {  -5,  2.5},
        {-2.5,  2.5},
        {-7.5,    5},
        {  -5,    5},
        {-2.5,    5},
        {-7.5,  7.5},
        {  -5,  7.5},
        {-2.5,  7.5}
    };

    Mesh embedded_mesh_2 (embedded_positions_2);

    const auto outside_nodes = barycentric_container.add_embedded_mesh(&embedded_mesh_2);
    EXPECT_EQ(outside_nodes, std::vector<UNSIGNED_INTEGER_TYPE>({0, 3, 6, 7, 8}));
}