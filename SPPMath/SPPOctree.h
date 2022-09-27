// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMath.h"
#include "SPPPrimitiveShapes.h"
#include <iostream>

namespace SPP
{
    namespace OctreeNodeType
	{
		static constexpr uint32_t const Node_Right = 0b001;
		static constexpr uint32_t const Node_Forward = 0b010;
		static constexpr uint32_t const Node_Up = 0b100;

		static constexpr uint32_t const TNW = 0b000;
		static constexpr uint32_t const TNE = 0b001;
		static constexpr uint32_t const TSW = 0b010;
		static constexpr uint32_t const TSE = 0b011;

		static constexpr uint32_t const BNW = 0b100;
		static constexpr uint32_t const BNE = 0b101;
		static constexpr uint32_t const BSW = 0b110;
		static constexpr uint32_t const BSE = 0b111;
	}          
      
    class IOctreeElement;

    //reference data
    //state of florida 721 by 582 km
    //2,147,483,648

    class SPP_MATH_API LooseOctree
    {
    public:
        struct NodeInfo
        {
            int32_t activeNodes = 0;
            int32_t elementCount = 0;
        };

        struct TileCoord
        {
            uint32_t x, y, z;
        };

        class LooseOctreeNode
        {
            LooseOctreeNode(const LooseOctreeNode&) = delete;
            LooseOctreeNode& operator=(const LooseOctreeNode&) = delete;

        private:
            std::list< const IOctreeElement* >  _elements;
            std::unique_ptr< LooseOctreeNode> _children[8];

        public:
            LooseOctreeNode() = default;

            void AddElement(IOctreeElement* InElement);
            void RemoveElement(IOctreeElement* InElement);

            bool CanDelete() const;

            LooseOctreeNode* GetPlacementNode(Vector3i InSearchCenter,
                int32_t InSearchExtent,
                int32_t InCurrentBoundExtents,
                uint8_t CurrentDepth = 0);

            void WalkElements(const AABBi& InAABB, const std::function<bool(const IOctreeElement*)>& InFunction,
                int32_t InCurrentBoundExtents,
                uint8_t CurrentDepth = 0);
           
            void WalkElements(const std::vector<Planed>& frustumPlanes,
                Vector3i InCurrentCenter, 
                const std::function<bool(const IOctreeElement*)>& InFunction,
                const std::function<bool(const Vector3i&, int32_t)>& InContinuation,
                int32_t InCurrentBoundExtents,
                uint8_t CurrentDepth = 0);

            void WalkNodes(const std::function<bool(const AABBi&)>& InFunction, 
                Vector3i InCurrentCenter, 
                int32_t InCurrentBoundExtents, 
                uint8_t CurrentDepth = 0);      

            void WalkElements(const LooseOctree *octree,
                const TileCoord& Coord,
                const std::function<bool(const AABBi&)>& InFilter,
                const std::function<bool(const IOctreeElement*)>& InFunction,
                uint8_t CurrentDepth = 0);

            void Report(std::vector<LooseOctree::NodeInfo>& inNodes, uint8_t CurrentDepth = 0) const;
            
            void ImageGeneration(const LooseOctree* octree,
                int32_t Width, 
                int32_t Height, 
                int32_t InCurrentBoundExtents, 
                std::vector<Color3>& oData,
                uint8_t UnitToPixelShift,
                const TileCoord& ParentCoord,
                const std::function<bool(const AABBi&)>& InFilter,
                uint8_t CurrentDepth = 0);
        };

    private:
        Vector3d _center;
        int32_t _extents = 0;
        uint8_t _extentsP2 = 0;
        uint8_t _maxDepth = 0;
        std::unique_ptr < LooseOctreeNode > _rootNode;
        Matrix4x4 _worldToOctree;

    public:    
        LooseOctree() = default;
        void Initialize(const Vector3d& center, const int32_t Extents, int32_t DesiredLowestExtents, const Matrix4x4 &InWorldToOctree = Matrix4x4::Identity() )
        { 
            _worldToOctree = InWorldToOctree;

            _center = center;
            _extents = roundUpToPow2(Extents);
            _extentsP2 = powerOf2(_extents);
            _maxDepth = powerOf2(_extents / DesiredLowestExtents);            
        }
        virtual ~LooseOctree() = default;

        void AddElement(IOctreeElement *InElement);
        void RemoveElement(IOctreeElement* InElement);
        
        void Report();
        void ImageGeneration(int32_t& oWidth, int32_t& oHeight, std::vector<Color3>& oData, const std::function<bool(const AABBi&)>& InFilter) const;

        inline int32_t GetExtentsAtDepth(uint8_t InDepth) const
        {
            return _extents >> InDepth;
        }

        inline int32_t GetDepthAtExtentSize(uint8_t InExtents) const
        {
            auto curExtents = roundUpToPow2(InExtents);
            return powerOf2(_extents / curExtents);
        }

        inline AABBi GetLooseAABB(const TileCoord& ParentCoord, uint8_t Depth) const;

        void WalkElements(const AABB &InAABB, const std::function<bool(const IOctreeElement *)> &InFunction, uint8_t MaxDepthToWalk = 0xFF);
        
        void WalkElements(const std::vector<Planed> &frustumPlanes, 
            const std::function<bool(const IOctreeElement*)>& InFunction,
            const std::function<bool(const Vector3i&, int32_t)>& InContinuation, 
            uint8_t MaxDepthToWalk = 0xFF);

        void WalkElements(const std::function<bool(const AABBi&)>& InFilter,
            const std::function<bool(const IOctreeElement*)>& InFunction);

        void WalkNodes(const std::function<bool(const AABBi&)>& InFunction, uint8_t MaxDepthToWalk = 0xFF);

        const Vector3d GetCenter() const { return _center; }
        uint8_t GetMaxDepth() const { return _maxDepth; }
    };

    using OctreeLinkPtr = const LooseOctree::LooseOctreeNode*;

    class SPP_MATH_API IOctreeElement
    {
    public:
        virtual Spherei GetBounds() const = 0;
        virtual void SetOctreeLink(OctreeLinkPtr InOctree) = 0;
        virtual const OctreeLinkPtr GetOctreeLink() = 0;
        virtual ~IOctreeElement() = default;
    };
}