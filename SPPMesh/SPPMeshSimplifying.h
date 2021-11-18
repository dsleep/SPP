// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMesh.h"
#include "ThreadPool.h"

namespace SPP
{
	SPP_MESH_API Vector3 barycentric(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c);
	SPP_MESH_API Vector2 interpolate(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c, const Vector2 attrs[3]);

	namespace Simplify
	{
		class SymetricMatrix
		{

		public:

			// Constructor

			SymetricMatrix(double c = 0) 
			{
				for (int i = 0; i < 10; ++i)
					m[i] = c; 
			}

			SymetricMatrix(double m11, double m12, double m13, double m14,
				double m22, double m23, double m24,
				double m33, double m34,
				double m44) {
				m[0] = m11;  m[1] = m12;  m[2] = m13;  m[3] = m14;
				m[4] = m22;  m[5] = m23;  m[6] = m24;
				m[7] = m33;  m[8] = m34;
				m[9] = m44;
			}

			// Make plane

			SymetricMatrix(double a, double b, double c, double d)
			{
				m[0] = a * a;  m[1] = a * b;  m[2] = a * c;  m[3] = a * d;
				m[4] = b * b;  m[5] = b * c;  m[6] = b * d;
				m[7] = c * c; m[8] = c * d;
				m[9] = d * d;
			}

			double operator[](int c) const { return m[c]; }

			// Determinant

			double det(int a11, int a12, int a13,
				int a21, int a22, int a23,
				int a31, int a32, int a33)
			{
				double det = m[a11] * m[a22] * m[a33] + m[a13] * m[a21] * m[a32] + m[a12] * m[a23] * m[a31]
					- m[a13] * m[a22] * m[a31] - m[a11] * m[a23] * m[a32] - m[a12] * m[a21] * m[a33];
				return det;
			}

			const SymetricMatrix operator+(const SymetricMatrix& n) const
			{
				return SymetricMatrix(m[0] + n[0], m[1] + n[1], m[2] + n[2], m[3] + n[3],
					m[4] + n[4], m[5] + n[5], m[6] + n[6],
					m[7] + n[7], m[8] + n[8],
					m[9] + n[9]);
			}

			SymetricMatrix& operator+=(const SymetricMatrix& n)
			{
				m[0] += n[0];   m[1] += n[1];   m[2] += n[2];   m[3] += n[3];
				m[4] += n[4];   m[5] += n[5];   m[6] += n[6];   m[7] += n[7];
				m[8] += n[8];   m[9] += n[9];
				return *this;
			}

			double m[10];
		};

		SPP_MESH_API double vertex_error(SymetricMatrix q, double x, double y, double z);

		/////////////////////////////////////////////
		//
		// Mesh Simplification Tutorial
		//
		// (C) by Sven Forstmann in 2014
		//
		// License : MIT
		// http://opensource.org/licenses/MIT
		//
		//https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
		//
		// 5/2016: Chris Rorden created minimal version for OSX/Linux/Windows compile
		// DS: ported to C++
		class FastQuadricMeshSimplification
		{
		private:

			enum Attributes
			{
				NONE,
				NORMAL = 2,
				TEXCOORD = 4,
				COLOR = 8
			};

			struct Triangle 
			{ 
				Vector3ui v;
				uint8_t material = 0;
				int attr = 0;

				Vector3 n;
				double err[4] = { 0 };
				bool dirty = false;
				bool deleted = false;

				std::atomic_bool bLocked = false;
				uint32_t owningThread = -1;

				Triangle() = default;
				Triangle(const Vector3ui &InIndices, uint8_t InMat, int InAttr ) : v(InIndices), material(InMat), attr(InAttr)
				{
				}

				Triangle(const Triangle& InValue)
				{
					*this = InValue;
				}

				void operator=(const Triangle& InValue)
				{
					v = InValue.v;
					material = InValue.material;
					attr = InValue.attr;

					memcpy(err, InValue.err, sizeof(uint32_t) * 3);
					n = InValue.n;

					dirty = InValue.dirty;
					deleted = InValue.deleted;
				}
			};

			struct Vertex 
			{ 
				Vector3 p;		
				Vector2 uvs;
				SymetricMatrix q;

				// first idx is tri, then tri position
				std::vector< std::tuple<uint32_t, uint8_t > > Tris;
				bool border = false; 
				bool bModified = false;

				Vertex() = default;
				Vertex(const Vector3 &InPos, const Vector2 &InUV) : p(InPos), uvs(InUV) {}

				Vertex(const Vertex& InValue)
				{
					*this = InValue;
				}

				void operator=(const Vertex& InValue)
				{					
					p = InValue.p;
					uvs = InValue.uvs;
					q = InValue.q;
					Tris = InValue.Tris;
					border = InValue.border;
					bModified = InValue.bModified;
				}
			};

			struct AtomicLocker
			{
				bool bDidLock = false;
				bool bIsOurs = false;
				std::atomic_bool& lock;
				uint32_t& owningThread;
				AtomicLocker(std::atomic_bool& inlock, uint32_t& InOwningThread) : lock(inlock), owningThread(InOwningThread)
				{
					bDidLock = (lock.exchange(true) == false);

					if (bDidLock == false)
					{
						if (GetCurrentThreadID() == owningThread)
						{
							bIsOurs = true;
						}
					}
					else
					{
						bIsOurs = true;
						owningThread = GetCurrentThreadID();
					}
				}
				operator bool() const
				{
					return bDidLock || bIsOurs;
				}
				~AtomicLocker()
				{
					if (bDidLock)
					{
						owningThread = (uint32_t)-1;
						lock.exchange(false);
					}
				}
			};

			std::shared_ptr< MeshTranslator> _translator;

			std::vector<Triangle> _triangles;
			std::vector<Vertex> _vertices;

			size_t orgVertCount = 0;
			size_t orgTriCount = 0;

			bool _bResizeVerts = true;

			static double calculate_error(const Vertex& Vert1, const Vertex& Vert2, Vector3& p_result);
			bool flipped(const Vector3 &p, int i0, int i1, const Vertex& v0, std::vector<uint32_t>& deleted);
			void update_uvs(Vertex& v, const Vector3& p, std::vector<bool>& deleted);
			void update_triangles(int combinedIndex, const Vertex& v, std::vector<uint32_t>& deleted, int& deleted_triangles);
			void update_mesh(int iteration);
			void compact_mesh();

			void _calculate_initial();
			void _calculate_borders();

			bool _lock_vert_and_linked_tris(Vertex& InVert, std::list< AtomicLocker >& locks);
			void _populate_from_mesh();
			void _resolve_to_mesh();

		public:
			FastQuadricMeshSimplification(std::shared_ptr< MeshTranslator> InTranslator) : _translator(InTranslator){}
			
			void simplify_mesh(int target_count, double agressiveness = 7, bool bNoVertReplace = false);
		};
	};

}