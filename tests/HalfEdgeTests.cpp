#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mesh/HalfEdgeMesh.h"
#include <glm/gtc/epsilon.hpp>

class HalfEdgeTestFixture: public ::testing::Test {
protected:
    void SetUp() override {
        /*
        Creates the a triangle mesh in the following shape:
        p4 --- p3 --- p2
          \   /  \  /
           p0 --- p1
        */
        std::vector<Vertex> vertices(5);
        vertices[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
        vertices[1].position = glm::vec3(1.0f, 0.0f, 0.0f);
        vertices[2].position = glm::vec3(2.0f, 1.0f, 0.0f);
        vertices[3].position = glm::vec3(0.5f, 2.0f, 0.0f);
        vertices[4].position = glm::vec3(-1.0f, 1.0f, 0.0f);
        std::vector<uint32_t> indices = {
            0, 3, 4,
            0, 1, 3,
            1, 2, 3
        };

        mesh.BuildFromIndexedMesh(vertices, indices);
    }
    HalfEdgeMesh mesh;
};

TEST_F(HalfEdgeTestFixture, TriangleMeshEulerCharacteristic) {
    EXPECT_EQ(mesh.GetVertices().size(), 5u);
    EXPECT_EQ(mesh.GetFaces().size(), 3u);
    EXPECT_EQ(mesh.GetHalfEdges().size(), 9u); // 9 interior half edges + 5 boundary half edges
    EXPECT_EQ(mesh.GetEulerCharacteristic(), 1);
}

TEST_F(HalfEdgeTestFixture, FlipEdge) {
    const std::vector<HE_HalfEdge>& halfEdges = mesh.GetHalfEdges();
    HE_HalfEdge h = halfEdges[0];
    EXPECT_EQ(h.origin, 0);
    EXPECT_EQ(halfEdges[h.next].origin, 3);
    mesh.FlipEdge(0);
    h = halfEdges[0];
    EXPECT_EQ(h.origin, 4);
    EXPECT_EQ(halfEdges[h.next].origin, 1);
    HE_HalfEdge t = halfEdges[h.twin];
	EXPECT_EQ(t.origin, 1);
	EXPECT_EQ(halfEdges[t.next].origin, 4);
}

TEST_F(HalfEdgeTestFixture, SplitEdge) {
    const std::vector<HE_HalfEdge>& halfEdges = mesh.GetHalfEdges();
    HE_HalfEdge h = halfEdges[0];
    EXPECT_EQ(h.origin, 0);
    EXPECT_EQ(halfEdges[h.next].origin, 3);
    glm::vec3 expectedPos(0.25f, 1.0f, 0.0f);
    uint32_t newVertexIdx = mesh.SplitEdge(0, expectedPos);
    EXPECT_EQ(mesh.GetVertices().size(), 6u);
    EXPECT_EQ(mesh.GetHalfEdges().size(), 15u); // 15 interior half edges + 5 boundary half edges
    const HE_Vertex v = mesh.GetVertices()[newVertexIdx];
    EXPECT_FLOAT_EQ(v.position.x, expectedPos.x);
    EXPECT_FLOAT_EQ(v.position.y, expectedPos.y);
    EXPECT_FLOAT_EQ(v.position.z, expectedPos.z);
}

TEST_F(HalfEdgeTestFixture, FaceAcrossEdge){
    EXPECT_EQ(mesh.GetFaceAcross(0), 1);
    EXPECT_EQ(mesh.GetFaceAcross(4), 2);
    EXPECT_EQ(mesh.GetFaceAcross(5), 0);
    EXPECT_EQ(mesh.GetFaceAcross(8), 1);
    EXPECT_EQ(mesh.GetFaceAcross(1), INVALID_INDEX);
    EXPECT_EQ(mesh.GetFaceAcross(2), INVALID_INDEX);
    EXPECT_EQ(mesh.GetFaceAcross(3), INVALID_INDEX);
    EXPECT_EQ(mesh.GetFaceAcross(6), INVALID_INDEX);
    EXPECT_EQ(mesh.GetFaceAcross(7), INVALID_INDEX);
}

TEST_F(HalfEdgeTestFixture, VertexNeighbors){
    auto nbrs = mesh.GetVertexNeighbors(0);
    EXPECT_EQ(nbrs.size(), 3u);
    uint32_t expected[] = {1, 3, 4};
    ASSERT_THAT(nbrs, ::testing::UnorderedElementsAreArray(expected));
}