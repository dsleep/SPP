// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMeshSimplifying.h"
#include "SPPMeshlets.h"
#include "SPPLogging.h"

#include "ThreadPool.h"

#define loopi(start_l,end_l) for ( int i=start_l;i<end_l;++i )
#define loopi(start_l,end_l) for ( int i=start_l;i<end_l;++i )
#define loopj(start_l,end_l) for ( int j=start_l;j<end_l;++j )
#define loopk(start_l,end_l) for ( int k=start_l;k<end_l;++k )

namespace SPP
{
	LogEntry LOG_MESHSIMP("MESHSIMPLIFY");

	Vector3 barycentric(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
	{
		Vector3 v0 = b - a;
		Vector3 v1 = c - a;
		Vector3 v2 = p - a;
		double d00 = v0.dot(v0);
		double d01 = v0.dot(v1);
		double d11 = v1.dot(v1);
		double d20 = v2.dot(v0);
		double d21 = v2.dot(v1);
		double denom = d00 * d11 - d01 * d01;
		double v = (d11 * d20 - d01 * d21) / denom;
		double w = (d00 * d21 - d01 * d20) / denom;
		double u = 1.0 - v - w;
		return Vector3(u, v, w);
	}

	Vector2 interpolate(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c, const Vector2 attrs[3])
	{
		Vector3 bary = barycentric(p, a, b, c);
		Vector2 out = Vector2(0, 0);
		out = out + attrs[0] * bary[0];
		out = out + attrs[1] * bary[1];
		out = out + attrs[2] * bary[2];
		return out;
	}

	namespace Simplify
	{
		// Error between vertex and Quadric
		double vertex_error(SymetricMatrix q, double x, double y, double z)
		{
			return q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x + q[4] * y * y
				+ 2 * q[5] * y * z + 2 * q[6] * y + q[7] * z * z + 2 * q[8] * z + q[9];
		}

		// Error for one edge
		double FastQuadricMeshSimplification::calculate_error(const Vertex &Vert1, const Vertex& Vert2, Vector3& p_result)
		{
			// compute interpolated vertex

			SymetricMatrix q = Vert1.q + Vert2.q;
			bool   border = Vert1.border & Vert2.border;
			double error = 0;
			double det = q.det(0, 1, 2, 1, 4, 5, 2, 5, 7);
			if (det != 0 && !border)
			{

				// q_delta is invertible
				p_result[0] = -1 / det * (q.det(1, 2, 3, 4, 5, 6, 5, 7, 8));	// vx = A41/det(q_delta)
				p_result[1] = 1 / det * (q.det(0, 2, 3, 1, 5, 6, 2, 7, 8));	// vy = A42/det(q_delta)
				p_result[2] = -1 / det * (q.det(0, 1, 3, 1, 4, 6, 2, 5, 8));	// vz = A43/det(q_delta)

				error = vertex_error(q, p_result[0], p_result[1], p_result[2]);
			}
			else
			{
				// det = 0 -> try to find best result
				const Vector3 &p1 = Vert1.p;
				const Vector3 &p2 = Vert2.p;
				Vector3 p3 = (p1 + p2) / 2;
				double error1 = vertex_error(q, p1[0], p1[1], p1[2]);
				double error2 = vertex_error(q, p2[0], p2[1], p2[2]);
				double error3 = vertex_error(q, p3[0], p3[1], p3[2]);
				error = std::min(error1, std::min(error2, error3));
				if (error1 == error) p_result = p1;
				if (error2 == error) p_result = p2;
				if (error3 == error) p_result = p3;
			}
			return error;
		}

		bool FastQuadricMeshSimplification::flipped(const Vector3& p, 
			int i0, int i1,
			const Vertex& v0, 
			std::vector<uint32_t>& deleted
			)
		{
			for (int32_t Iter = 0; Iter < v0.Tris.size(); Iter++)
			{
				Triangle& t = _triangles[std::get<0>(v0.Tris[Iter])];
				if (t.deleted)
				{
					continue;
				}

				auto s = std::get<1>(v0.Tris[Iter]);
				auto id1 = t.v[(s + 1) % 3];
				auto id2 = t.v[(s + 2) % 3];

				if (id1 == i1 || id2 == i1) // delete ?
				{
					deleted[Iter] = 1;
					continue;
				}

				Vector3 d1 = _vertices[id1].p - p;
				d1.normalize();
				Vector3 d2 = _vertices[id2].p - p;
				d2.normalize();
				if (fabs(d1.dot(d2)) > 0.999)
				{
					return true;
				}
				Vector3 n = d1.cross(d2);
				n.normalize();
				if (n.dot(t.n) < 0.2)
				{
					return true;
				}
			}

			return false;
		}

		void FastQuadricMeshSimplification::update_uvs(Vertex& v, const Vector3& p, std::vector<bool >& deleted)
		{
			for (int32_t Iter = 0; Iter < v.Tris.size(); Iter++)
			{
				Triangle& t = _triangles[std::get<0>(v.Tris[Iter])];
				if (t.deleted)continue;
				if (deleted[Iter])continue;
				auto& p1 = _vertices[t.v[0]];
				auto& p2 = _vertices[t.v[1]];
				auto& p3 = _vertices[t.v[2]];
				Vector2 ptuvs[] = { p1.uvs, p2.uvs, p3.uvs };
				v.uvs = interpolate(p, p1.p, p2.p, p3.p, ptuvs);
				break;
			}
		}

		void FastQuadricMeshSimplification::update_triangles(int combinedIndex, const Vertex& v, std::vector<uint32_t>& deleted, int& deleted_triangles)
		{
			Vector3 p;

			for (int32_t Iter = 0; Iter < v.Tris.size(); Iter++)
			{
				Triangle& t = _triangles[std::get<0>(v.Tris[Iter])];
				if (t.deleted)continue;
				if (deleted[Iter])
				{
					t.deleted = 1;
					deleted_triangles++;
					continue;
				}
				t.v[std::get<1>(v.Tris[Iter])] = combinedIndex;
				t.dirty = 1;
				t.err[0] = calculate_error(_vertices[t.v[0]], _vertices[t.v[1]], p);
				t.err[1] = calculate_error(_vertices[t.v[1]], _vertices[t.v[2]], p);
				t.err[2] = calculate_error(_vertices[t.v[2]], _vertices[t.v[0]], p);
				t.err[3] = std::min(t.err[0], std::min(t.err[1], t.err[2]));
			}
		}

		template <typename IterType, typename Func>
		void parallel_for(IterType beg, IterType end, Func &InFunc, int32_t MaxJobs = -1, bool bRunSync = false)
		{
			auto eleCount = end - beg;
			
			size_t MaxTasks = (MaxJobs > 0) ? std::min<size_t>(MaxJobs, CPUThreaPool->WorkerCount()) : CPUThreaPool->WorkerCount();
			if (eleCount > 0)
			{
				if (bRunSync)
				{
					while (beg < end)
					{
						InFunc(*beg);
						beg++;
					}
				}
				else
				{
					auto elePart = std::max<decltype(eleCount)>(1, (eleCount + (MaxTasks - 1)) / MaxTasks );

					std::list<std::future<bool>> futures;
					for (int32_t Iter = 0; Iter < MaxTasks; Iter++)
					{
						auto threadStart = beg + (Iter * elePart);

						if (threadStart >= end)
						{
							break;
						}

						auto threadEnd = beg + std::min<decltype(eleCount)>(eleCount, (Iter + 1) * elePart);
						futures.push_back(CPUThreaPool->enqueue([&, localStart = threadStart, localEnd = threadEnd]() mutable -> bool
							{
								while (localStart < localEnd)
								{
									InFunc(*localStart);
									localStart++;
								}
								return true;
							}));
					}
					for (auto& curFuture : futures)
					{
						curFuture.wait();
					}
				}
			}
		}

		void FastQuadricMeshSimplification::_calculate_initial()
		{
			auto calcInitial = std::chrono::high_resolution_clock::now();

			for (auto& vert : _vertices)
			{
				vert.q = SymetricMatrix(0.0);
			}

			for (auto& t : _triangles)
			{
				Vector3 n, p[3];
				for (int32_t j = 0; j < 3; j++)
				{
					p[j] = _vertices[t.v[j]].p;
				}
				n = (p[1] - p[0]).cross(p[2] - p[0]);
				n.normalize();
				t.n = n;
				for (int32_t j = 0; j < 3; j++)
				{
					_vertices[t.v[j]].q = _vertices[t.v[j]].q + SymetricMatrix(n[0], n[1], n[2], -n.dot(p[0]));
				}
			}

			// use q to generate error term
			for (auto& t : _triangles)
			{
				// Calc Edge Error
				Vector3 p;
				for (int32_t j = 0; j < 3; j++)
				{
					t.err[j] = calculate_error(
						_vertices[t.v[j]],
						_vertices[t.v[(j + 1) % 3]], p);
				}
				t.err[3] = std::min(t.err[0], std::min(t.err[1], t.err[2]));
			}

#if 0
			parallel_for(_triangles.begin(), _triangles.end(),
				[&](Triangle& t)
				{
					// Calc Edge Error
					Vector3 p;
					for (int32_t j = 0; j < 3; j++)
					{
						t.err[j] = calculate_error(
							_vertices[t.v[j]],
							_vertices[t.v[(j + 1) % 3]], p);
					}
					t.err[3] = std::min(t.err[0], std::min(t.err[1], t.err[2]));
				});

#else




#endif

			SPP_LOG(LOG_MESHSIMP, LOG_INFO, " - _calculate_initial Complete %f", TimeSince(calcInitial));
		}

		void FastQuadricMeshSimplification::_calculate_borders()
		{
			auto timeStart = std::chrono::high_resolution_clock::now();

			std::vector<int> vcount, vids;

			for (auto& vert : _vertices)
			{
				vert.border = 0;
			}

			for (auto& v : _vertices)
			{
				vcount.clear();
				vids.clear();

				for (auto& triIter : v.Tris)
				{
					Triangle& t = _triangles[std::get<0>(triIter)];
					loopk(0, 3)
					{
						int ofs = 0, id = t.v[k];
						while (ofs < vcount.size())
						{
							if (vids[ofs] == id)break;
							ofs++;
						}
						if (ofs == vcount.size())
						{
							vcount.push_back(1);
							vids.push_back(id);
						}
						else
							vcount[ofs]++;
					}
				}
				loopj(0, vcount.size())
				{
					if (vcount[j] == 1)
					{
						_vertices[vids[j]].border = 1;
					}
				}
			}


			SPP_LOG(LOG_MESHSIMP, LOG_INFO, " - _calculate_borders Complete %f", TimeSince(timeStart));
		}

		void FastQuadricMeshSimplification::update_mesh(int iteration)
		{
			if (iteration > 0) // compact triangles
			{
				int dst = 0;
				for (auto& t : _triangles)
				{
					if (!t.deleted)
					{
						_triangles[dst++] = t;
					}
				}
				_triangles.resize(dst);
			}

			// Init Reference ID list	
			for (auto& vert : _vertices)
			{
				vert.Tris.clear();
			}

			for (int32_t Iter = 0; Iter < _triangles.size(); Iter++)
			{
				auto& curTri = _triangles[Iter];
				for (int8_t ptIter = 0; ptIter < 3; ptIter++)
				{
					_vertices[curTri.v[ptIter]].Tris.push_back(std::make_tuple(Iter, ptIter));
				}
			}			

			if (iteration == 0)
			{
				//
				// Init Quadrics by Plane & Edge Errors
				//
				// required at the beginning ( iteration == 0 )
				// recomputing during the simplification is not required,
				// but mostly improves the result for closed meshes
				//
				_calculate_initial();

				// Identify boundary : _vertices[].border=0,1
				_calculate_borders();
			}
		}

		void FastQuadricMeshSimplification::_populate_from_mesh()
		{			
			auto timeStart = std::chrono::high_resolution_clock::now();

			auto vertCount = _translator->GetVertexCount();
			_vertices.resize(vertCount);
			for (uint32_t Iter = 0; Iter < _translator->GetVertexCount(); Iter++)
			{
				_vertices[Iter] = Vertex( _translator->GetVertexPosition(Iter), _translator->GetVertexUV(Iter,0) );
			}

			auto indexCount = _translator->GetIndexCount();
			auto triCount = indexCount / 3;
			// really sets of 3
			SE_ASSERT(triCount * 3 == indexCount);
			_triangles.resize(triCount);
			for (size_t Iter = 0; Iter < triCount; Iter++)
			{
				auto idx0 = _translator->GetIndex((Iter * 3) + 0);
				auto idx1 = _translator->GetIndex((Iter * 3) + 1);
				auto idx2 = _translator->GetIndex((Iter * 3) + 2);

				_triangles[Iter] =
					Triangle
					(
						Vector3ui(idx0, idx1, idx2),
						0,
						TEXCOORD
					);
			}

			//if (EdgeGuard)
			//{
			//	auto edgesToGuard = IndexResource->GetSpan<int32_t>();

			//	for (size_t Iter = 0; Iter < edgesToGuard.GetCount(); Iter++)
			//	{
			//		auto curEdge = edgesToGuard[Iter];
			//		auto curTriangle = curEdge / 3;
			//		auto curTriIdx = curEdge % 3;

			//		_triangles[curTriangle].edgeGuard[curTriIdx] = true;
			//	}
			//}

			orgVertCount = vertCount;
			orgTriCount = triCount;

			SPP_LOG(LOG_MESHSIMP, LOG_INFO, " - _populate_from_mesh Complete %f", TimeSince(timeStart));
		}

		void FastQuadricMeshSimplification::_resolve_to_mesh()
		{
			if (_bResizeVerts == false)
			{
				std::unordered_map<uint32_t, uint32_t> vertToVert;
				{
					std::vector<Vertex> mergedVertices;
					for (size_t Iter = 0; Iter < _vertices.size(); Iter++)
					{
						auto& curVert = _vertices[Iter];
						if (curVert.bModified && curVert.Tris.empty() == false)
						{
							vertToVert[Iter] = mergedVertices.size() + _vertices.size();
							mergedVertices.push_back(curVert);
						}
					}
					_vertices.insert(_vertices.end(), mergedVertices.begin(), mergedVertices.end());
				}

				for (size_t Iter = 0; Iter < _triangles.size(); Iter++)
				{
					for (auto& idx : _triangles[Iter].v)
					{
						auto findRemap = vertToVert.find(idx);
						if (findRemap != vertToVert.end())
						{
							idx = findRemap->second;
						}
					}
				}
			}

			_translator->ResizeVertices(_vertices.size());
			_translator->ResizeIndices(_triangles.size() * 3);

			uint32_t currentVertIdx = _bResizeVerts ? 0 : orgVertCount;
			for (size_t Iter = currentVertIdx; Iter < _vertices.size(); Iter++)
			{
				//Vector3 position;
				//Vector3 normal;
				//Vector3 tangent;
				//Vector3 bitangent;
				//Vector2 texcoord;
				_translator->GetVertexPosition(Iter) = _vertices[Iter].p;
				_translator->GetVertexUV(Iter,0) = _vertices[Iter].uvs;
			}

			for (size_t Iter = 0; Iter < _triangles.size(); Iter++)
			{
				auto idx0 = _triangles[Iter].v[0];
				auto idx1 = _triangles[Iter].v[1];
				auto idx2 = _triangles[Iter].v[2];

				_translator->GetIndex(Iter * 3 + 0) = idx0;
				_translator->GetIndex(Iter * 3 + 1) = idx1;
				_translator->GetIndex(Iter * 3 + 2) = idx2;

				_translator->GetVertexNormal(idx0) = _triangles[Iter].n;
				_translator->GetVertexNormal(idx1) = _triangles[Iter].n;
				_translator->GetVertexNormal(idx2) = _triangles[Iter].n;
			}
		}

		void FastQuadricMeshSimplification::compact_mesh()
		{
			for (auto& curVer : _vertices)
			{
				// clear with reserver?
				curVer.Tris.clear();
			}

			uint32_t dst = 0;
			for (int32_t Iter = 0; Iter < _triangles.size(); Iter++)
			{
				if (!_triangles[Iter].deleted)
				{
					Triangle& t = _triangles[Iter];
					for (int8_t ptIter = 0; ptIter < 3; ptIter++)
					{
						_vertices[t.v[ptIter]].Tris.push_back(std::make_tuple(dst, ptIter));
					}
					if (dst != Iter)
					{
						_triangles[dst] = t;
					}
					dst++;
				}
			}
			_triangles.resize(dst);

			if (_bResizeVerts)
			{
				uint32_t currentVertIdx = 0;
				for (size_t Iter = 0; Iter < _vertices.size(); Iter++)
				{
					auto& curVer = _vertices[Iter];
					if (curVer.Tris.empty() == false)
					{
						_vertices[currentVertIdx] = curVer;

						for (int32_t Iter = 0; Iter < _vertices[currentVertIdx].Tris.size(); Iter++)
						{
							Triangle& t = _triangles[std::get<0>(_vertices[currentVertIdx].Tris[Iter])];
							auto vertIdx = std::get<1>(_vertices[currentVertIdx].Tris[Iter]);
							t.v[vertIdx] = currentVertIdx;
						}
						currentVertIdx++;
					}
				}
				_vertices.resize(currentVertIdx);
			}
		}

				
		bool FastQuadricMeshSimplification::_lock_vert_and_linked_tris(Vertex& vIn, std::list< AtomicLocker > &locks)
		{
			for (int32_t Iter = 0; Iter < vIn.Tris.size(); Iter++)
			{
				Triangle& t = _triangles[std::get<0>(vIn.Tris[Iter])];
				auto& newLock = locks.emplace_back(t.bLocked, t.owningThread);
				if (newLock == false) 
				{
					return false;
				}
			}
			return true;
		}

		void FastQuadricMeshSimplification::simplify_mesh(int target_count, double agressiveness, bool bNoVertReplace)
		{
			_populate_from_mesh();

			SPP_LOG(LOG_MESHSIMP, LOG_INFO, "FastQuadricMeshSimplification::simplify_mesh triangles %d target count %d", _triangles.size(), target_count);

			auto indexCount = _translator->GetIndexCount();
			const uint32_t triCount = indexCount / 3;
			std::unordered_map< EdgeKey, AdjTri, EdgeKey::HASH > edgeHash;
			using edgeHasIterator = std::unordered_map<EdgeKey, AdjTri, EdgeKey::HASH >::iterator;

			for (uint32_t triIter = 0; triIter < triCount; ++triIter)
			{
				uint32_t index = triIter * 3;

				for (uint32_t iEdge = 0; iEdge < 3; ++iEdge)
				{
					uint32_t i0 = _translator->GetIndex(index + ((iEdge + 0) % 3));
					uint32_t i1 = _translator->GetIndex(index + ((iEdge + 1) % 3));

					std::pair<edgeHasIterator, bool> sharedInsert = edgeHash.insert({ EdgeKey(i0, i1), AdjTri() });
					auto& curAdjTri = sharedInsert.first->second;

					SE_ASSERT(curAdjTri.triIdx < ARRAY_SIZE(curAdjTri.connectedTris));
					curAdjTri.connectedTris[curAdjTri.triIdx++] = triIter;
				}
			}

			_bResizeVerts = !bNoVertReplace;

			// init
			for(auto &curTri : _triangles)
			{
				curTri.deleted = false;
			}

			// main iteration loop
			int deleted_triangles = 0;
			auto originalTriCount = _triangles.size();
			//int iteration = 0;
			//loop(iteration,0,100)
			for (int iteration = 0; iteration < 100; iteration++)
			{
				if (originalTriCount - deleted_triangles <= target_count)break;

				SPP_LOG(LOG_MESHSIMP, LOG_INFO, " - Iteration %d DELETED: %d", iteration, deleted_triangles);

				// update mesh once in a while
				update_mesh(iteration);

				// clear dirty flag
				for (auto& curTri : _triangles)
				{
					curTri.dirty = false;
				}

				//
				// All triangles with edges below the threshold will be removed
				//
				// The following numbers works well for most models.
				// If it does not, try to adjust the 3 parameters
				//
				double threshold = 0.000000001 * pow(double(iteration) + 3, agressiveness);

				std::atomic_uint32_t currentTriIdx = 0;
				std::list<std::future<bool>> futures;
				for (int32_t Iter = 0; Iter < CPUThreaPool->WorkerCount(); Iter++)
				{
					futures.push_back(CPUThreaPool->enqueue([&]() -> bool
						{								
							while (true)
							{
								auto currentIdx = currentTriIdx.fetch_add(1);
								if (currentIdx >= _triangles.size())
								{
									return true;
								}

								auto &curTri = _triangles[currentIdx];
											
								std::list< AtomicLocker > locks;
								auto &lockTri = locks.emplace_back(curTri.bLocked, curTri.owningThread);

								// lock it, if already locked we done
								if (lockTri == false)
								{
									continue;
								}

								if (curTri.err[3] > threshold) continue;
								if (curTri.deleted) continue;
								if (curTri.dirty) continue;

								auto i0 = curTri.v[0];
								auto i1 = curTri.v[1];
								auto i2 = curTri.v[2];

								Vector3ui triIndices(i0, i1, i2);
								Vertex* trivertices[3] = { &_vertices[i0], &_vertices[i1], &_vertices[i2] };

								for (int32_t j = 0; j < 3; j++)
								{
									if (curTri.err[j] < threshold)
									{
										auto i0 = triIndices[j];
										auto i1 = triIndices[(j + 1) % 3];
										Vertex& v0 = *trivertices[j];
										Vertex& v1 = *trivertices[(j + 1) % 3];

										//edge worth collapsing
										if (_lock_vert_and_linked_tris(v0, locks) == false ||
											_lock_vert_and_linked_tris(v1, locks) == false)
										{
											break;
										}													

										std::vector<uint32_t> deleted0, deleted1;
										deleted0.resize(v0.Tris.size()); // normals temporarily
										deleted1.resize(v1.Tris.size()); // normals temporarily

										// Border check
										//if (_translator->IsVertexProtected(i0) || _translator->IsVertexProtected(i1))
										//{
										//	continue;
										//}
										if (v0.border || v1.border)  continue;

										// Compute vertex to collapse to
										Vector3 p;
										calculate_error(v0, v1, p);
										// don't remove if flipped
										if (flipped(p, i0, i1, v0, deleted0)) continue;
										if (flipped(p, i1, i0, v1, deleted1)) continue;

										//if ((curTri.attr & TEXCOORD) == TEXCOORD)
										//{
										//	update_uvs(v0, p, deleted0);
										//}

										// not flipped, so remove edge
										v0.p = p;
										v0.q = v1.q + v0.q;
										v0.bModified = true;

										update_triangles(i0, v0, deleted0, deleted_triangles);
										update_triangles(i0, v1, deleted1, deleted_triangles);								
										
										break;
									}
								}

								if (originalTriCount - deleted_triangles <= target_count)
								{
									return true;
								}
							}

							return true;
						}));
				}

				for (auto& curFuture : futures)
				{
					curFuture.wait();
				}	

		
			}
			// clean up mesh
			compact_mesh();

			// set mesh
			_resolve_to_mesh();

		} //simplify_mesh()
	}
}