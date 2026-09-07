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
    EXPECT_EQ(h.origin, khepri::VertexId{0});
    EXPECT_EQ(halfEdges[h.next.Get()].origin, khepri::VertexId{3});
    mesh.FlipEdge(0);
    h = halfEdges[0];
    EXPECT_EQ(h.origin, khepri::VertexId{4});
    EXPECT_EQ(halfEdges[h.next.Get()].origin, khepri::VertexId{1});
    HE_HalfEdge t = halfEdges[h.twin.Get()];
    EXPECT_EQ(t.origin, khepri::VertexId{1});
    EXPECT_EQ(halfEdges[t.next.Get()].origin, khepri::VertexId{4});
}

TEST_F(HalfEdgeTestFixture, SplitEdge) {
    const std::vector<HE_HalfEdge>& halfEdges = mesh.GetHalfEdges();
    HE_HalfEdge h = halfEdges[0];
    EXPECT_EQ(h.origin, khepri::VertexId{0});
    EXPECT_EQ(halfEdges[h.next.Get()].origin, khepri::VertexId{3});
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

TEST_F(HalfEdgeTestFixture, BakeToRenderMeshOutputsCorrectCounts) {
    std::vector<Vertex> bakedVertices;
    std::vector<uint32_t> bakedIndices;
    mesh.BakeToRenderMesh(bakedVertices, bakedIndices);

    EXPECT_EQ(bakedVertices.size(), 5u);
    EXPECT_EQ(bakedIndices.size(), 9u);
}

TEST_F(HalfEdgeTestFixture, StrongIdVertexNeighborRingWalk) {
    auto nbrIds = mesh.GetVertexNeighborIds(khepri::VertexId{0});
    EXPECT_EQ(nbrIds.size(), 3u);
    std::vector<uint32_t> rawIds;
    for (auto id : nbrIds) rawIds.push_back(id.Get());
    uint32_t expected[] = {1, 3, 4};
    EXPECT_THAT(rawIds, ::testing::UnorderedElementsAreArray(expected));

    // Vertex 3 is connected to vertices 0, 1, 2, 4
    auto v3Nbrs = mesh.GetVertexNeighborIds(khepri::VertexId{3});
    EXPECT_EQ(v3Nbrs.size(), 4u);
    std::vector<uint32_t> rawV3;
    for (auto id : v3Nbrs) rawV3.push_back(id.Get());
    uint32_t expectedV3[] = {0, 1, 2, 4};
    EXPECT_THAT(rawV3, ::testing::UnorderedElementsAreArray(expectedV3));
}

TEST_F(HalfEdgeTestFixture, ConstQueriesAndAccessors) {
    const HalfEdgeMesh& constMesh = mesh;
    EXPECT_EQ(constMesh.GetVertexCount(), 5u);
    EXPECT_EQ(constMesh.GetFaceCount(), 3u);
    EXPECT_EQ(constMesh.GetHalfEdgeCount(), 9u);
    EXPECT_EQ(constMesh.GetVertexPosition(khepri::VertexId{0}), glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(constMesh.GetVertexPosition(khepri::VertexId{1}), glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST_F(HalfEdgeTestFixture, StrongIdTypeSafetyAndSentinels) {
    khepri::VertexId v0{0};
    khepri::VertexId v1{1};
    khepri::VertexId vInvalid;

    EXPECT_TRUE(v0.IsValid());
    EXPECT_TRUE(v1.IsValid());
    EXPECT_FALSE(vInvalid.IsValid());
    EXPECT_NE(v0, v1);
    EXPECT_EQ(v0, khepri::VertexId{0});

    khepri::HalfEdgeId he0{0};
    khepri::FaceId f0{0};
    EXPECT_EQ(he0.Get(), 0u);
    EXPECT_EQ(f0.Get(), 0u);
}