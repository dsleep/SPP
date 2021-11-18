// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMeshlets.h"
#include "SPPMeshSimplifying.h"
#include "SPPString.h"
#include "metis.h"
#include "ThreadPool.h"
#include <functional>
#include <algorithm> 
#include <iostream>
#include <fstream>
#include <exception>
	

//#include "SPPAssetCache.h"

namespace SPP
{
	LogEntry LOG_MESHLET("MESHLET");

	void ValidateAdjacency(std::vector<uint32_t>& AdjacencyLayout, std::vector<uint32_t>& Adjacency)
	{
		auto GraphVertexCount = AdjacencyLayout.size() - 1;
		std::vector< std::unordered_set< uint32_t > > GraphVertices;
		GraphVertices.resize(GraphVertexCount);

		for (uint32_t Iter = 0; Iter < GraphVertexCount; Iter += 2)
		{
			auto& startIdx = AdjacencyLayout[Iter];
			auto& endIdx = AdjacencyLayout[Iter + 1];

			for (uint32_t AdjIter = startIdx; AdjIter < endIdx; AdjIter++)
			{
				GraphVertices[Iter].insert(Adjacency[AdjIter]);
			}			
		}

		for (uint32_t Iter = 0; Iter < GraphVertexCount; Iter += 2)
		{
			auto& currentSet = GraphVertices[Iter];
			for (auto& currentIdx : currentSet)
			{
				if (GraphVertices[currentIdx].count(Iter) != 1)
				{
					SPP_LOG(LOG_MESHLET, LOG_INFO, "Missing %d in %d", Iter, currentIdx);
				}
			}
		}
	}

	void QuickBuildMultiTriangleAdjacencyList(std::shared_ptr< MeshTranslator> InTranslator,
		std::vector<uint32_t>& AdjacencyLayout,
		std::vector<uint32_t>& Adjacency)
	{
		auto indexCount = InTranslator->GetIndexCount();

		const uint32_t triCount = indexCount / 3;

		std::unordered_map< EdgeKey, AdjTri, EdgeKey::HASH > edgeHash;
		using edgeHasIterator = std::unordered_map<EdgeKey, AdjTri, EdgeKey::HASH >::iterator;

		for (uint32_t triIter = 0; triIter < triCount; ++triIter)
		{
			uint32_t index = triIter * 3;

			for (uint32_t iEdge = 0; iEdge < 3; ++iEdge)
			{
				uint32_t i0 = InTranslator->GetIndex(index + ((iEdge + 0) % 3));
				uint32_t i1 = InTranslator->GetIndex(index + ((iEdge + 1) % 3));

				std::pair<edgeHasIterator, bool> sharedInsert = edgeHash.insert({ EdgeKey(i0, i1), AdjTri() });
				auto& curAdjTri = sharedInsert.first->second;

				SE_ASSERT(curAdjTri.triIdx < ARRAY_SIZE(curAdjTri.connectedTris));
				curAdjTri.connectedTris[curAdjTri.triIdx++] = triIter;				
			}
		}

		uint32_t IsolatedTris = 0;
		// Initialize the adjacency list
		Adjacency.clear();
		AdjacencyLayout.resize(triCount + 1);
		for (uint32_t triIter = 0; triIter < triCount; ++triIter)
		{
			AdjacencyLayout[triIter] = Adjacency.size();
			uint32_t index = triIter * 3;

			for (uint32_t iEdge = 0; iEdge < 3; ++iEdge)
			{				
				uint32_t i0 = InTranslator->GetIndex(index + ((iEdge + 0) % 3));
				uint32_t i1 = InTranslator->GetIndex(index + ((iEdge + 1) % 3));

				auto foundEdge = edgeHash.find(EdgeKey(i0, i1));
				SE_ASSERT(foundEdge != edgeHash.end());
				auto& curAdjTri = foundEdge->second;				

				for (int32_t matches = 0; matches < curAdjTri.triIdx; matches++)
				{
					auto& thisMatchedTri = curAdjTri.connectedTris[matches];
					if (thisMatchedTri != triIter)
					{
						Adjacency.push_back(thisMatchedTri);						
					}
				}
			}

			// none were added, so isolated
			if (AdjacencyLayout[triIter] == Adjacency.size())
			{
				IsolatedTris++;
			}
		}
		AdjacencyLayout[triCount] = Adjacency.size();

		//ValidateAdjacency(AdjacencyLayout, Adjacency);

		SPP_LOG(LOG_MESHLET, LOG_INFO, "Built Adjacencies Edge Count %d, Isolated tris %d", Adjacency.size() / 2, IsolatedTris);
	}
		
	struct GraphLayer
	{
		idx_t VertexCount;
		std::vector<idx_t> AdjLayout;
		std::vector<idx_t> Adj;
		std::vector<idx_t> Partition;
		std::vector< std::unordered_set<idx_t> > partitionToVert;

		uint32_t LayerIndex = 0;
		std::weak_ptr< GraphLayer > Parent;
		std::shared_ptr< GraphLayer > Child;
	};

	struct MeshletGraphNode
	{
		AABB Bounds;
		uint32_t TriangleCountTotal = 0;
		std::vector< MetaMeshlet > Meshlets;

		std::weak_ptr< MeshletGraphNode > Parent;
		std::vector<std::shared_ptr< MeshletGraphNode > > Children;

		//
		uint32_t LayerPartition;
		std::weak_ptr< GraphLayer > LayerLink;		
	};

	void Parallel_MetisGraphOfPartsIntoMeshlets(std::shared_ptr< MeshTranslator> InTranslator, 
		idx_t PartitionCount,
		const std::vector<idx_t> &Parts, 
		uint32_t InMaxVerts, 
		uint32_t InMaxPrims,
		uint32_t InIndexOffset,
		std::vector< MetaMeshlet > &oMeshlets )
	{	
		auto currentMeshletOffset = oMeshlets.size();
		oMeshlets.resize(PartitionCount + currentMeshletOffset);

		auto triCount = InTranslator->GetIndexCount() / 3;

		auto startMeshlets = std::chrono::high_resolution_clock::now();
		std::atomic_uint32_t currentMeshletIdx = 0;
		std::atomic_uint32_t failedAdd = 0;
		std::mutex meshletMutex;
		std::list<std::future<bool>> futures;

		for (int32_t Iter = 0; Iter < CPUThreaPool->WorkerCount(); Iter++)
		{
			futures.push_back(CPUThreaPool->enqueue([&]() -> bool
				{
					while (true)
					{
						auto currentIdx = currentMeshletIdx.fetch_add(1);

						if (currentIdx >= PartitionCount)
						{
							return true;
						}

						if (currentIdx % 1000 == 0)
						{
							SPP_LOG(LOG_MESHLET, LOG_INFO, "BUILDING %%%f of tris (%d:%d)", ((float)currentIdx / (float)PartitionCount) * 100.0f, currentIdx, PartitionCount);
						}

						for (uint32_t triIter = 0; triIter < triCount; triIter++)
						{
							if (Parts[triIter] == currentIdx)
							{
								uint32_t tri[3] =
								{
									InTranslator->GetIndex(triIter * 3),
									InTranslator->GetIndex(triIter * 3 + 1),
									InTranslator->GetIndex(triIter * 3 + 2),
								};

								if (AddToMeshlet(InMaxVerts, InMaxPrims, oMeshlets[currentIdx + currentMeshletOffset], tri, triIter + InIndexOffset) == false)
								{									
									failedAdd++;
								}
							}
						}
					}

					return true;
				}));
		}

		for (auto& curFuture : futures)
		{
			curFuture.wait();
		}

		SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLET Complete %f seconds FAILS %d", TimeSince(startMeshlets), failedAdd.load());
	}

	void WriteGraphFile(const std::vector<idx_t>& AdjacencyLayout, const std::vector<idx_t>& Adjacency)
	{
		SE_ASSERT((Adjacency.size() % 2) == 0);

		auto vertexCount = AdjacencyLayout.size() - 1;
		auto edgeCount = Adjacency.size() / 2;

		std::ofstream outputFile("MetisFile.graph");

		outputFile << std::string_format("%%VERT COUNT AND EDGES\n", vertexCount, edgeCount);
		outputFile << std::string_format("%d %d\n", vertexCount, edgeCount);
		outputFile << std::string("%%EDGE LIST\n");

		for (uint32_t Iter = 0; Iter < AdjacencyLayout.size() - 1; Iter += 2)
		{
			auto& startIdx = AdjacencyLayout[Iter];
			auto& endIdx = AdjacencyLayout[Iter + 1];

			for (uint32_t AdjIter = startIdx; AdjIter < endIdx; AdjIter++)
			{
				outputFile << std::string_format("%d ", Adjacency[AdjIter]);
			}

			outputFile << std::string_format("\n", vertexCount, edgeCount);
		}

		outputFile.close();
	}

	idx_t BuildGraphForMeshlets(std::shared_ptr< MeshTranslator> InTranslator,
		const std::vector<uint32_t>& AdjacencyLayout,
		const std::vector<uint32_t>& Adjacency,
		std::vector<idx_t>& oParts)
	{
		auto triCount = (InTranslator->GetIndexCount() / 3);
		//trying new method
		idx_t nvtxs = triCount;

		SE_ASSERT(AdjacencyLayout.size() == (triCount + 1));

		std::vector<idx_t> xadj(AdjacencyLayout.begin(), AdjacencyLayout.end());
		std::vector<idx_t> adjncy(Adjacency.begin(), Adjacency.end());

		idx_t ncon = 1;
		idx_t nParts = triCount / 60;
		idx_t objval = 0;

		oParts.resize(triCount);

		//WriteGraphFile(xadj, adjncy);
			
		SPP_LOG(LOG_MESHLET, LOG_INFO, "Building METIS Graph for meshlet...");
		auto startMetis = std::chrono::high_resolution_clock::now();
		auto metisValue = METIS_PartGraphKway(&nvtxs, &ncon, xadj.data(), adjncy.data(),
			nullptr, nullptr, nullptr, &nParts, nullptr, nullptr, nullptr,
			&objval, oParts.data());
		SPP_LOG(LOG_MESHLET, LOG_INFO, "METIS Complete %f seconds PASSED %d", TimeSince(startMetis), (metisValue == METIS_OK));
		

		return nParts;
	}
		
	std::shared_ptr< GraphLayer> GetParentGraphVertsFromChildren(std::shared_ptr<GraphLayer> InGraphLayer,
		std::unordered_set<idx_t>& InChildVerts, 
		std::unordered_set<idx_t>& oParentVerts)
	{
		// child vertices are the parents partitions....
		if (auto parentLayer = InGraphLayer->Parent.lock())
		{
			std::unordered_set<idx_t> parentVerts;
			auto& parentPartitionToVert = parentLayer->partitionToVert;

			for (auto& childVert : InChildVerts)
			{
				auto& graphVertSet = parentPartitionToVert[childVert];
				oParentVerts.insert(graphVertSet.begin(), graphVertSet.end());
			}
			return parentLayer;
		}
		
		return nullptr;
	}

	void RecurseUpGetVerts(std::shared_ptr< GraphLayer> InGraphLayer, std::unordered_set<idx_t> &InChildVerts, std::vector<idx_t> &oVerts )
	{
		// child vertices are the parents partitions....
		std::unordered_set< idx_t > ParentVerts;
		auto parentLayer = GetParentGraphVertsFromChildren(InGraphLayer, InChildVerts, ParentVerts);

		if(parentLayer)
		{
			RecurseUpGetVerts(parentLayer, ParentVerts, oVerts);
		}
		else
		{
			oVerts.insert(oVerts.end(), InChildVerts.begin(), InChildVerts.end());
		}
	}

	void RunMeshSimplifier(std::shared_ptr< MeshTranslator> SimplifiedLayer, uint32_t DesiredTriCount)
	{
		SPP_LOG(LOG_MESHLET, LOG_INFO, "Running Mesh Simplifier");
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - verts %d", SimplifiedLayer->GetVertexCount());
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - indices %d tris %d", SimplifiedLayer->GetIndexCount(), SimplifiedLayer->GetIndexCount() / 3);
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - desired tri count %d", DesiredTriCount);

		auto startSimplifying = std::chrono::high_resolution_clock::now();
		Simplify::FastQuadricMeshSimplification meshSimplification(SimplifiedLayer);
		meshSimplification.simplify_mesh(DesiredTriCount, 7.0, true);
		
		SPP_LOG(LOG_MESHLET, LOG_INFO, "     - REDUCED verts %d", SimplifiedLayer->GetVertexCount());
		SPP_LOG(LOG_MESHLET, LOG_INFO, "     - REDUCED indices %d tris %d", SimplifiedLayer->GetIndexCount(), SimplifiedLayer->GetIndexCount() / 3);
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - Simplifying Complete %f seconds", TimeSince(startSimplifying));
	}

	void MeshIntoMeshlets(std::shared_ptr< MeshTranslator> InMeshTranslator,
		std::vector< MetaMeshlet >& oMeshlets,
		uint32_t InMaxVerts,
		uint32_t InMaxPrims)
	{
		// get simplified mesh adjacencies
		std::vector<uint32_t> AdjacencyLayout;
		std::vector<uint32_t> Adjacency;
		QuickBuildMultiTriangleAdjacencyList(InMeshTranslator, AdjacencyLayout, Adjacency);

		// partition for meshlest
		std::vector<idx_t> GraphVertexToPartition;
		auto PartitionCount = BuildGraphForMeshlets(InMeshTranslator, AdjacencyLayout, Adjacency, GraphVertexToPartition);

		Parallel_MetisGraphOfPartsIntoMeshlets(InMeshTranslator, PartitionCount, GraphVertexToPartition, InMaxVerts, InMaxPrims, 0, oMeshlets);
	}	

	void BuildLODHierarchy(std::shared_ptr< MeshTranslator> InMeshTranslator,
		std::shared_ptr<GraphLayer> InCurrentLayer,
		std::shared_ptr<MeshletGraphNode> InGraphNode,
		uint32_t InMaxVerts,
		uint32_t InMaxPrims,
		std::vector<float> &LayerReductions )
	{
		//CreateMeshletGraph(InTopLayer, InTopGraphNode);

		auto GraphParentLayer = InCurrentLayer->Parent.lock();
		auto PartitionCount = InCurrentLayer->partitionToVert.size();
		
		{
			std::vector<idx_t> Tris;
			RecurseUpGetVerts(InCurrentLayer, InCurrentLayer->partitionToVert[InGraphNode->LayerPartition ], Tris);

			auto simplifiedLayer = InMeshTranslator->CreateCopy(true, false);
			simplifiedLayer->ResizeIndices(Tris.size() * 3);
			
			uint32_t curIdx = 0;
			for (auto& curTri : Tris)
			{
				simplifiedLayer->GetIndex(curIdx++) = InMeshTranslator->GetIndex((curTri * 3) + 0);
				simplifiedLayer->GetIndex(curIdx++) = InMeshTranslator->GetIndex((curTri * 3) + 1);
				simplifiedLayer->GetIndex(curIdx++) = InMeshTranslator->GetIndex((curTri * 3) + 2);
			}

			float SimplifiedAmmount = LayerReductions[InCurrentLayer->LayerIndex];

			// create simplified mesh
			if (SimplifiedAmmount < 1.0f)
			{
				RunMeshSimplifier(simplifiedLayer, Tris.size() * SimplifiedAmmount);
			}
			// create meshlets
			MeshIntoMeshlets(simplifiedLayer, InGraphNode->Meshlets, InMaxVerts, InMaxPrims);
			InGraphNode->Bounds = simplifiedLayer->GetBounds();
			InGraphNode->TriangleCountTotal = simplifiedLayer->GetIndexCount() / 3;
		}

		// we are at the end
		if (!GraphParentLayer)
		{
			return;
		}

		auto& VertSet = InCurrentLayer->partitionToVert[InGraphNode->LayerPartition];
		
		auto ChildVertNum = VertSet.size();
		InGraphNode->Children.resize(VertSet.size());
		
		uint32_t ChildIter = 0;
		for(auto &currentVert : VertSet)
		{
			InGraphNode->Children[ChildIter] = std::make_shared< MeshletGraphNode>();
			InGraphNode->Children[ChildIter]->LayerPartition = currentVert;
			InGraphNode->Children[ChildIter]->LayerLink = InCurrentLayer;

			// graph layers go up, graph nodes go down
			BuildLODHierarchy(InMeshTranslator, GraphParentLayer, InGraphNode->Children[ChildIter], InMaxVerts, InMaxPrims, LayerReductions);
			ChildIter++;
		}  
	}

	static const char* TabAsSpaces[] = 
		{ 
		"",
		"    ",  
		"        ",
		"            ",
		"                ",
		"                    " };

	void MetisRecurseAndReduce(std::shared_ptr< GraphLayer> CurrentGrapLayer, 
		idx_t DesiredNumberOfPartitions,
		idx_t DesiredMinPartitions, 
		int32_t CurrentLayer)
	{
		idx_t nvtxs = CurrentGrapLayer->VertexCount;
		CurrentGrapLayer->LayerIndex = CurrentLayer;

		idx_t ncon = 1;
		idx_t nParts = DesiredNumberOfPartitions;
		idx_t objval = 0;
		auto& part = CurrentGrapLayer->Partition;
		part.resize(nvtxs);

		auto &currentLayerAdjLayout = CurrentGrapLayer->AdjLayout;
		auto &currentLayerAdj = CurrentGrapLayer->Adj;

		SPP_LOG(LOG_MESHLET, LOG_INFO, "%sBuilding Partitioned Reduced Graph LAYER: %d", TabAsSpaces[CurrentLayer], CurrentLayer);
		SPP_LOG(LOG_MESHLET, LOG_INFO, "%s - verts: %d", TabAsSpaces[CurrentLayer], nvtxs);
		SPP_LOG(LOG_MESHLET, LOG_INFO, "%s - adj: %d", TabAsSpaces[CurrentLayer], currentLayerAdj.size());
		SPP_LOG(LOG_MESHLET, LOG_INFO, "%s - desired partitions: %d", TabAsSpaces[CurrentLayer], nParts);

		//WriteGraphFile(currentLayerAdjLayout, currentLayerAdj);

		auto startMetis = std::chrono::high_resolution_clock::now();
		auto metisValue = METIS_PartGraphRecursive(&nvtxs, &ncon, currentLayerAdjLayout.data(), currentLayerAdj.data(),
			nullptr, nullptr, nullptr, &nParts, nullptr, nullptr, nullptr,
			&objval, part.data());
		SPP_LOG(LOG_MESHLET, LOG_INFO, "%sMETIS Complete %f seconds PASSED %d", TabAsSpaces[CurrentLayer], TimeSince(startMetis), (metisValue == METIS_OK));
			
		auto& partitionToVert = CurrentGrapLayer->partitionToVert;
		partitionToVert.resize(nParts);
		for (uint32_t vertIter = 0; vertIter < nvtxs; vertIter++)
		{
			partitionToVert[part[vertIter]].insert(vertIter);
		}

		if (DesiredNumberOfPartitions <= DesiredMinPartitions)
		{
			std::shared_ptr< GraphLayer > LowestLayer = CurrentGrapLayer;
			while (LowestLayer->Child)
			{
				LowestLayer = LowestLayer->Child;
			}

			//one more on the chain
			LowestLayer->Child = std::make_shared< GraphLayer >();
			LowestLayer->Child->Parent = LowestLayer;
			LowestLayer = LowestLayer->Child;
			LowestLayer->LayerIndex = CurrentLayer + 1;

			LowestLayer->VertexCount = LowestLayer->Parent.lock()->partitionToVert.size();
			LowestLayer->partitionToVert.resize(1);
			for (int32_t Iter = 0; Iter < LowestLayer->VertexCount; Iter++)
			{
				LowestLayer->partitionToVert[0].insert(Iter);
			}

			return;
		}
		DesiredNumberOfPartitions = DesiredNumberOfPartitions >> 2;

		CurrentGrapLayer->Child = std::make_shared< GraphLayer>();
		CurrentGrapLayer->Child->Parent = CurrentGrapLayer;

		auto& graphChild = *CurrentGrapLayer->Child;
		graphChild.VertexCount = nParts;

		auto& ChildAdjLayout = graphChild.AdjLayout;
		auto& ChildAdj = graphChild.Adj;
				
		ChildAdjLayout.resize(nParts + 1);
		for (uint32_t partIter = 0; partIter < nParts; partIter++)
		{
			ChildAdjLayout[partIter] = ChildAdj.size();
			auto& curPart = partitionToVert[partIter];

			std::unordered_set<idx_t> localPartitionedAdj;
			for (auto& curVert : curPart)
			{
				auto startIdx = currentLayerAdjLayout[curVert];
				auto endIdx = currentLayerAdjLayout[curVert + 1];
								
				for (uint32_t adjIter = startIdx; adjIter < endIdx; adjIter++)
				{
					auto adjVert = currentLayerAdj[adjIter];
					auto adjVertPart = part[adjVert];
					//
					if (partIter != adjVertPart)
					{
						localPartitionedAdj.insert(adjVertPart);
					}
				}				
			}
			ChildAdj.insert(ChildAdj.end(), localPartitionedAdj.begin(), localPartitionedAdj.end());
		}
		ChildAdjLayout[nParts] = ChildAdj.size();

		MetisRecurseAndReduce(CurrentGrapLayer->Child, DesiredNumberOfPartitions, DesiredMinPartitions, CurrentLayer+1);
	}

	Meshletizer::Meshletizer(const char *InAssetPath, std::shared_ptr< MeshTranslator> InTranslator, uint32_t InMaxVerts, uint32_t InMaxPrims)
	{
		SE_ASSERT(InTranslator);
		SPP_LOG(LOG_MESHLET, LOG_INFO, "Meshletizer::Meshletizer VertCount %d TriCount %d MaxVerts %d MaxPrimts %d",
			InTranslator->GetVertexCount(),
			InTranslator->GetIndexCount() / 3,
			InMaxVerts, InMaxPrims);

		//_assetPath = InAssetPath;
		_translator = InTranslator;
		_maxVerts = InMaxVerts;
		_maxPrims = InMaxPrims;		

		//if (auto findBounds = GetCachedAsset(_assetPath, "Bounds"))
		//{
		//	BinaryBlobSerializer& blobAsset = *findBounds;			
		//	blobAsset >> _bounds;
		//}
		//else
		{			
			_bounds = InTranslator->GetBounds();
			BinaryBlobSerializer outCachedAsset;
			outCachedAsset << _bounds;
			//PutCachedAsset(_assetPath, outCachedAsset, "Bounds");
		}

		auto boundsExtents = _bounds.Extent();
		auto maxBoundsExtent = std::max(boundsExtents[0], std::max(boundsExtents[1], boundsExtents[2]));
		auto rescale = 32768.0f / maxBoundsExtent;
		auto translationCenter = _bounds.Center();

		// 
		Matrix4x4 worldToOctree = Matrix4x4::Identity();
		worldToOctree(0, 0) = rescale; // scale the x coordinates of the projected point 
		worldToOctree(1, 1) = rescale; // scale the y coordinates of the projected point 
		worldToOctree(2, 2) = rescale;
		worldToOctree.block<1, 3>(3, 0) = -Vector3(translationCenter[0], translationCenter[1], translationCenter[2]);
	}

	void MeshGraphToArray(std::shared_ptr<MeshletGraphNode> InGraphNode,
		uint32_t CurNodeIdx,
		std::vector< MeshNode > &oNodes,
		std::vector< MetaMeshlet >  &oMeshlets)
	{
		oNodes[CurNodeIdx].Bounds = InGraphNode->Bounds;
		oNodes[CurNodeIdx].TriCount = InGraphNode->TriangleCountTotal;

		// Meshlets
		std::get<0>(oNodes[CurNodeIdx].MeshletRange) = oMeshlets.size();
		oMeshlets.insert(oMeshlets.end(), InGraphNode->Meshlets.begin(), InGraphNode->Meshlets.end());
		std::get<1>(oNodes[CurNodeIdx].MeshletRange) = oMeshlets.size();
		// Children
		if (InGraphNode->Children.size())
		{
			auto ChildStartIdx = oNodes.size();
			std::get<0>(oNodes[CurNodeIdx].ChildrenRange) = ChildStartIdx;
			oNodes.resize(oNodes.size() + InGraphNode->Children.size());
			std::get<1>(oNodes[CurNodeIdx].ChildrenRange) = oNodes.size();

			for (uint32_t Iter = 0; Iter < InGraphNode->Children.size(); Iter++)
			{
				MeshGraphToArray(InGraphNode->Children[Iter],
					Iter + ChildStartIdx,
					oNodes,
					oMeshlets);
				oNodes[CurNodeIdx].ChildTriCount += InGraphNode->Children[Iter]->TriangleCountTotal;
			}
		}
	}

	void Meshletizer::_Meshletize()
	{
		//if (auto cachedAsset = GetCachedAsset(_assetPath, "NodalMeshletsV2"))
		//{
		//	BinaryBlobSerializer& blobAsset = *cachedAsset;
		//	_translator->Load(blobAsset);
		//	blobAsset >> _nodes;
		//	blobAsset >> _meshlets;
		//}
		//else
		{
			auto vertexCount = _translator->GetVertexCount();
			auto indexCount = _translator->GetIndexCount();
			auto triCount = indexCount / 3;


			auto triGraphLayer = std::make_shared<GraphLayer>();


			SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLETIZE PHASE 1 (build master adj, and master graph layers)");
			{
				auto startPhase = std::chrono::high_resolution_clock::now();

				std::vector<uint32_t> AdjacencyLayout;
				std::vector<uint32_t> Adjacency;
				QuickBuildMultiTriangleAdjacencyList(_translator, AdjacencyLayout, Adjacency);

				triGraphLayer->VertexCount = triCount;
				triGraphLayer->AdjLayout.insert(triGraphLayer->AdjLayout.end(), AdjacencyLayout.begin(), AdjacencyLayout.end());
				triGraphLayer->Adj.insert(triGraphLayer->Adj.end(), Adjacency.begin(), Adjacency.end());

				auto initialMeshCount = triCount / 50000;
				// 
				MetisRecurseAndReduce(triGraphLayer, initialMeshCount, 5, 0);

				SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLET PHASE 1 COMPLETE %f seconds", TimeSince(startPhase));
			}

			// layer report
			{
				SPP_LOG(LOG_MESHLET, LOG_INFO, "GRAPH LAYERS REPORT");

				uint32_t LayerIncr = 0;
				auto graphCurIter = triGraphLayer;
				while (graphCurIter)
				{
					SPP_LOG(LOG_MESHLET, LOG_INFO, "LAYER_%d", LayerIncr);
					SPP_LOG(LOG_MESHLET, LOG_INFO, " - VERTS %d", graphCurIter->VertexCount);
					SPP_LOG(LOG_MESHLET, LOG_INFO, " - PARTITIONS %d", graphCurIter->partitionToVert.size());
					LayerIncr++;

					graphCurIter = graphCurIter->Child;
				}
			}

			auto TopLevelGraphNode = std::make_shared< MeshletGraphNode >();

			SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLETIZE PHASE 2 (***MAIN PHASE*** initial nodes into simplified meshlet subsets)");
			{
				auto startPhase = std::chrono::high_resolution_clock::now();

				std::shared_ptr< GraphLayer > LowestLayer = triGraphLayer;
				while (LowestLayer->Child)
				{
					LowestLayer = LowestLayer->Child;
				}

				std::vector<float> LayerReductions;
				LayerReductions.push_back(1.0f);
				LayerReductions.push_back(0.66f);
				LayerReductions.push_back(0.33f);
				LayerReductions.push_back(0.1f);

				BuildLODHierarchy(_translator, LowestLayer, TopLevelGraphNode, _maxVerts, _maxPrims, LayerReductions);

				SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLET PHASE 2 COMPLETE %f seconds", TimeSince(startPhase));
			}						

			SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLETIZE PHASE 3 (graph into array)");
			{
				auto startPhase = std::chrono::high_resolution_clock::now();

				MeshNode& curNode = _nodes.emplace_back();
				MeshGraphToArray(TopLevelGraphNode, 0, _nodes, _meshlets);

				SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLET PHASE 3 COMPLETE %f seconds", TimeSince(startPhase));
			}
			
			BinaryBlobSerializer outCachedAsset;
			_translator->Store(outCachedAsset);
			outCachedAsset << _nodes;
			outCachedAsset << _meshlets;
			//PutCachedAsset(_assetPath, outCachedAsset, "NodalMeshletsV2");
			
			//SPP_LOG(LOG_MESHLET, LOG_INFO, " - Total Tri Counts %d", TotalTriCount);
			//SPP_LOG(LOG_MESHLET, LOG_INFO, " - Coarsified Count %d", NewTotalTriCount);
			SPP_LOG(LOG_MESHLET, LOG_INFO, " - Meshlets %d", _meshlets.size());
		}
	}

	bool Meshletizer::ComputeMeshlets(std::vector<Subset>& meshletSubsets,
		std::vector<Meshlet>& meshlets,
		std::vector<MeshNode>& meshnodes,
		std::vector<uint8_t>& uniqueVertexIndices,
		std::vector<PackedTriangle>& primitiveIndices)
	{
		_Meshletize();
		
		Subset meshletSubset;

		//	Subset meshletSubset;
		meshletSubset.Offset = static_cast<uint32_t>(meshlets.size());
		meshletSubset.Count = static_cast<uint32_t>(_meshlets.size());
		meshletSubsets.push_back(meshletSubset);

		// copy nodes over
		std::swap(meshnodes, _nodes);

		//	// Determine final unique vertex index and primitive index counts & offsets.
		uint32_t startVertCount = static_cast<uint32_t>(uniqueVertexIndices.size()) / sizeof(uint32_t);
		uint32_t startPrimCount = static_cast<uint32_t>(primitiveIndices.size());

		uint32_t uniqueVertexIndexCount = startVertCount;
		uint32_t primitiveIndexCount = startPrimCount;

		// Resize the meshlet output array to hold the newly formed meshlets.
		uint32_t meshletCount = static_cast<uint32_t>(meshlets.size());
		meshlets.resize(meshletCount + _meshlets.size());

		std::vector<InlineMeshlet<uint32_t>> builtMeshlets;
		builtMeshlets.reserve(_meshlets.size());
		for (auto& curMeshlet : _meshlets)
		{
			builtMeshlets.push_back(curMeshlet.ToInline());
		}

		for (uint32_t j = 0, dest = meshletCount; j < static_cast<uint32_t>(builtMeshlets.size()); ++j, ++dest)
		{
			meshlets[dest].VertOffset = uniqueVertexIndexCount;
			meshlets[dest].VertCount = static_cast<uint32_t>(builtMeshlets[j].UniqueVertexIndices.size());
			uniqueVertexIndexCount += static_cast<uint32_t>(builtMeshlets[j].UniqueVertexIndices.size());

			meshlets[dest].PrimOffset = primitiveIndexCount;
			meshlets[dest].PrimCount = static_cast<uint32_t>(builtMeshlets[j].PrimitiveIndices.size());
			primitiveIndexCount += static_cast<uint32_t>(builtMeshlets[j].PrimitiveIndices.size());
		}

		// Allocate space for the new data.
		uniqueVertexIndices.resize(uniqueVertexIndexCount * sizeof(uint32_t));
		primitiveIndices.resize(primitiveIndexCount);

		// Copy data from the freshly built meshlets into the output buffers.
		auto vertDest = reinterpret_cast<uint32_t*>(uniqueVertexIndices.data()) + startVertCount;
		auto primDest = reinterpret_cast<uint32_t*>(primitiveIndices.data()) + startPrimCount;

		for (uint32_t j = 0; j < static_cast<uint32_t>(builtMeshlets.size()); ++j)
		{
			std::memcpy(vertDest, builtMeshlets[j].UniqueVertexIndices.data(), builtMeshlets[j].UniqueVertexIndices.size() * sizeof(uint32_t));
			std::memcpy(primDest, builtMeshlets[j].PrimitiveIndices.data(), builtMeshlets[j].PrimitiveIndices.size() * sizeof(uint32_t));

			vertDest += builtMeshlets[j].UniqueVertexIndices.size();
			primDest += builtMeshlets[j].PrimitiveIndices.size();
		}

		SPP_LOG(LOG_MESHLET, LOG_INFO, "MESHLET COMPUTE REPORT");//% s", *_assetPath);
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - meshlets %d", builtMeshlets.size());
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - meshlet nodes %d", meshnodes.size());
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - uniqueVertexIndices count %d", uniqueVertexIndices.size() / sizeof(uint32_t));
		SPP_LOG(LOG_MESHLET, LOG_INFO, " - PrimitiveIndices count %d", primitiveIndices.size());

		return true;
	}
}