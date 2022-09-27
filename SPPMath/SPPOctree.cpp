// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPOctree.h"
#include "SPPLogging.h"

namespace SPP
{
    LogEntry LOG_OCT("OCTREE");

    LooseOctree::LooseOctreeNode* LooseOctree::LooseOctreeNode::GetPlacementNode(
        Vector3i InSearchCenter,
        int32_t InSearchExtent,
        int32_t InCurrentBoundExtents,
        uint8_t DepthDecrement)    
    {
        auto childExt = InCurrentBoundExtents >> 1;
         
        if (InSearchExtent >= childExt || DepthDecrement == 0)
        {
            return this;
        }

        int32_t whichnode = 0;
        if (InSearchCenter[0] > 0) whichnode |= OctreeNodeType::Node_Right;
        if (InSearchCenter[1] > 0) whichnode |= OctreeNodeType::Node_Up;
        if (InSearchCenter[2] > 0) whichnode |= OctreeNodeType::Node_Forward;

        SE_ASSERT(whichnode < 8);    

        auto childCenterOffset = Vector3i
            ((whichnode & OctreeNodeType::Node_Right) ? childExt : -childExt,
             (whichnode & OctreeNodeType::Node_Up) ? childExt : -childExt,
             (whichnode & OctreeNodeType::Node_Forward) ? childExt : -childExt);

        if (!_children[whichnode])
        {
            _children[whichnode] = std::make_unique<LooseOctreeNode>();
        }

        return _children[whichnode]->GetPlacementNode(InSearchCenter - childCenterOffset, InSearchExtent, childExt, DepthDecrement - 1);
    }

    void LooseOctree::LooseOctreeNode::WalkElements(const AABBi& InAABB,
        const std::function<bool(const IOctreeElement *)>& InFunction,
        int32_t InCurrentBoundExtents,
        uint8_t CurrentDepth)
    {
        if (CurrentDepth == 0)
        {
            return;
        }

        auto childExt = InCurrentBoundExtents >> 1;
        auto looseChildExt = childExt + (childExt >> 1);

        for (auto& ele : _elements)
        {
            if (InFunction(ele) == false)
            {
                return;
            }
        }

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                auto childCenterOffset = Vector3i
                    ((ChildIter & OctreeNodeType::Node_Right) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Up) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Forward) ? childExt : -childExt);

                AABBi shiftedAABB = InAABB.Translate(-childCenterOffset);
                      
                if (Intersects_AAABBi_to_UniformCenteredAABB(shiftedAABB, looseChildExt))
                {
                    _children[ChildIter]->WalkElements(shiftedAABB, InFunction, childExt, CurrentDepth - 1);
                }
            }
        }
    }

    void LooseOctree::LooseOctreeNode::WalkElements(const std::vector<Planed>& frustumPlanes,
        Vector3i InCurrentCenter,
        const std::function<bool(const IOctreeElement*)>& InFunction,
        const std::function<bool(const Vector3i&, int32_t)>& InContinuation,
        int32_t InCurrentBoundExtents,
        uint8_t CurrentDepth)
    {
        if (CurrentDepth == 0 || 
            InContinuation(InCurrentCenter, InCurrentBoundExtents) == false)
        {
            return;
        }

        auto childExt = InCurrentBoundExtents >> 1;
        auto looseChildExt = childExt + (childExt >> 1);

        for (auto& ele : _elements)
        {
            if (InFunction(ele) == false)
            {
                return;
            }
        }

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                auto childCenterOffset = Vector3i
                ((ChildIter & OctreeNodeType::Node_Right) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Up) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Forward) ? childExt : -childExt);
                 
                Vector3i childCenter = (InCurrentCenter + childCenterOffset);
                AABBi childAABB{ 
                    childCenter - Vector3i(looseChildExt,looseChildExt,looseChildExt),
                    childCenter + Vector3i(looseChildExt,looseChildExt,looseChildExt)
                     };

                if (BoxInFrustum< Vector3i >(frustumPlanes, childAABB))
                {
                    _children[ChildIter]->WalkElements(frustumPlanes, childCenter, InFunction, InContinuation, childExt, CurrentDepth - 1);
                }                
            }
        }
    }

    void LooseOctree::LooseOctreeNode::WalkElements(const LooseOctree* octree, 
        const TileCoord& Coord,
        const std::function<bool(const AABBi&)>& InFilter,
        const std::function<bool(const IOctreeElement*)>& InFunction,
        uint8_t CurrentDepth)
    {
        auto curBounds = octree->GetLooseAABB(Coord, CurrentDepth);

        if (!InFilter(curBounds))
        {
            return;
        }

        for (auto& ele : _elements)
        {
            if (InFunction(ele) == false)
            {
                return;
            }
        }

        TileCoord childStart{ Coord.x << 1, Coord.y << 1, Coord.z << 1 };
        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                auto ChildCoord = TileCoord{
                    (ChildIter & OctreeNodeType::Node_Right) ? childStart.x + 1 : childStart.x,
                    (ChildIter & OctreeNodeType::Node_Up) ? childStart.y + 1 : childStart.y,
                    (ChildIter & OctreeNodeType::Node_Forward) ? childStart.z + 1 : childStart.z };

                _children[ChildIter]->WalkElements(octree, ChildCoord, InFilter, InFunction, CurrentDepth + 1);
            }
        }
    }

    void LooseOctree::LooseOctreeNode::WalkNodes(const std::function<bool(const AABBi&)>& InFunction,
        Vector3i InCurrentCenter,
        int32_t InCurrentBoundExtents,
        uint8_t CurrentDepth)
    {
        if (CurrentDepth == 0)
        {
            return;
        }

        AABBi ourAABB{
            InCurrentCenter - Vector3i(InCurrentBoundExtents,InCurrentBoundExtents,InCurrentBoundExtents),
            InCurrentCenter + Vector3i(InCurrentBoundExtents,InCurrentBoundExtents,InCurrentBoundExtents)
        };
               
        if (InFunction(ourAABB) == false)
        {
            return;            
        }

        auto childExt = InCurrentBoundExtents >> 1;
        auto looseChildExt = childExt + (childExt >> 1);

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                auto childCenterOffset = Vector3i
                ((ChildIter & OctreeNodeType::Node_Right) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Up) ? childExt : -childExt,
                    (ChildIter & OctreeNodeType::Node_Forward) ? childExt : -childExt);

                Vector3i childCenter = (InCurrentCenter + childCenterOffset);
                AABBi childAABB{
                    childCenter - Vector3i(looseChildExt,looseChildExt,looseChildExt),
                    childCenter + Vector3i(looseChildExt,looseChildExt,looseChildExt)
                };
                
                _children[ChildIter]->WalkNodes(InFunction, childCenter, childExt, CurrentDepth - 1);
            }
        }
    }

    void LooseOctree::LooseOctreeNode::Report(std::vector<LooseOctree::NodeInfo>& inNodes, uint8_t CurrentDepth) const
    {
        inNodes[CurrentDepth].activeNodes++;
        inNodes[CurrentDepth].elementCount += (int32_t) _elements.size();

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                _children[ChildIter]->Report(inNodes, CurrentDepth + 1);
            }
        }
    }

    void AddRectColor(int32_t Width,
        int32_t Height,
        const Vector2i &PixelStart, const Vector2i &PixelEnd,
        uint8_t Value,
        std::vector<Color3>& oData)
    {
        for (int32_t PixelY = PixelStart[1]; PixelY < PixelEnd[1]; PixelY++)
        {
            if (PixelY < 0 || PixelY >= Height)
            {
                continue;
            }
            int32_t RowStart = PixelY * Width;
            for (int32_t PixelX = PixelStart[0]; PixelX < PixelEnd[0]; PixelX++)
            {
                if (PixelX < 0 || PixelX >= Width)
                {
                    continue;
                }
                auto& curColor = oData[RowStart + PixelX];
                curColor[0] = (uint8_t)std::min(255, (uint16_t)curColor[0] + Value);
                curColor[1] = (uint8_t)std::min(255, (uint16_t)curColor[1] + Value);
                curColor[2] = (uint8_t)std::min(255, (uint16_t)curColor[2] + Value);
            }
        }
    }

    void LooseOctree::LooseOctreeNode::ImageGeneration(const LooseOctree* octree,
        int32_t Width, 
        int32_t Height, 
        int32_t InCurrentBoundExtents,
        std::vector<Color3>& oData,
        uint8_t UnitToPixelShift, const TileCoord &CurCoord,
        const std::function<bool(const AABBi&)>& InFilter,
        uint8_t CurrentDepth)
    {
        auto BoundsSize = InCurrentBoundExtents << 1;
        auto PixelCnt = BoundsSize >> UnitToPixelShift;
        auto AddLoose = InCurrentBoundExtents;
        auto AddLoosePixelCnt = AddLoose >> UnitToPixelShift;

        auto curBounds = octree->GetLooseAABB(CurCoord, CurrentDepth);

        if (!InFilter(curBounds))
        {
            return;
        }

        Vector2i PixelStart = Vector2i{ CurCoord.x * PixelCnt, CurCoord.z * PixelCnt  };
        PixelStart -= Vector2i{ (AddLoosePixelCnt >> 1), (AddLoosePixelCnt >> 1) };
        Vector2i PixelEnd = PixelStart +
            Vector2i{ PixelCnt + AddLoosePixelCnt, PixelCnt + AddLoosePixelCnt };

        AddRectColor(Width, Height, PixelStart, PixelEnd, CurrentDepth, oData );

        TileCoord childStart{ CurCoord.x << 1, CurCoord.y << 1, CurCoord.z << 1 };
        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                auto ChildCoord = TileCoord{
                    (ChildIter & OctreeNodeType::Node_Right) ? childStart.x + 1 : childStart.x,
                    (ChildIter & OctreeNodeType::Node_Up) ? childStart.y + 1 : childStart.y,
                    (ChildIter & OctreeNodeType::Node_Forward) ? childStart.z + 1 : childStart.z };
                             

                _children[ChildIter]->ImageGeneration(octree,
                    Width, Height, InCurrentBoundExtents >> 1, oData, UnitToPixelShift, ChildCoord, InFilter, CurrentDepth+1);
            }
        }
    }

    void LooseOctree::LooseOctreeNode::AddElement(IOctreeElement* InElement)
    {
        _elements.push_back(InElement);
        InElement->SetOctreeLink(this);
    }

    bool LooseOctree::LooseOctreeNode::CanDelete() const
    {
        if (!_elements.empty())
        {
            return false;
        }

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter] && !_children[ChildIter]->CanDelete())
            {
                return false;
            }
        }

        return true;
    }

    void LooseOctree::LooseOctreeNode::RemoveElement(IOctreeElement* InElement)
    {
        SE_ASSERT(InElement->GetOctreeLink() == this);
        InElement->SetOctreeLink(nullptr);
        _elements.remove(InElement);     

        // prune?
    }

    AABBi LooseOctree::GetLooseAABB(const TileCoord& ParentCoord, uint8_t Depth) const
    {
        auto CurExtent = GetExtentsAtDepth(Depth);
        auto CurLength = (CurExtent << 1);
        auto CurExtentLooseAdd = (CurExtent >> 1);
        auto CurExtentLoose = CurExtent + CurExtentLooseAdd;

		return AABBi(
			Vector3i{
			    (int32_t)(ParentCoord.x * CurLength - _extents - CurExtentLooseAdd),
			    (int32_t)(ParentCoord.y * CurLength - _extents - CurExtentLooseAdd),
			    (int32_t)(ParentCoord.z * CurLength - _extents - CurExtentLooseAdd) },
			Vector3i{
                (int32_t)(ParentCoord.x * CurLength - _extents - CurExtentLooseAdd) + (CurExtentLoose << 1),
				(int32_t)(ParentCoord.y * CurLength - _extents - CurExtentLooseAdd) + (CurExtentLoose << 1),
				(int32_t)(ParentCoord.z * CurLength - _extents - CurExtentLooseAdd) + (CurExtentLoose << 1) });
	}

    void LooseOctree::AddElement(IOctreeElement *InElement)
    {
        if (!_rootNode)
        {
            _rootNode = std::make_unique<LooseOctreeNode>();
        }
        SE_ASSERT(InElement->GetOctreeLink() == nullptr);

        auto EleBounds = InElement->GetBounds();
        auto placementNode = _rootNode->GetPlacementNode(EleBounds.center, EleBounds.extent, _extents, _maxDepth);
        if (placementNode)
        {
            placementNode->AddElement(InElement);
        }
        else
        {
            //report
        }
    }

    void LooseOctree::WalkElements(const AABB& InAABB, const std::function<bool(const IOctreeElement *)>& InFunction, uint8_t MaxDepthToWalk)
    {       
        if (_rootNode)
        {
            AABBi aabbI = Convert(InAABB);
            _rootNode->WalkElements(aabbI, InFunction, _extents, MaxDepthToWalk);
        }        
    }

    void LooseOctree::WalkElements(const std::function<bool(const AABBi&)>& InFilter,
        const std::function<bool(const IOctreeElement*)>& InFunction)
    {
        if (_rootNode)
        {
            _rootNode->WalkElements(this, TileCoord{ 0,0 }, InFilter, InFunction);
        }
    }
        
    void LooseOctree::WalkElements(const std::vector<Planed>& frustumPlanes,
        const std::function<bool(const IOctreeElement*)>& InFunction,
        const std::function<bool(const Vector3i&, int32_t)>& InContinuation,
        uint8_t MaxDepthToWalk)
    {
        if (_rootNode)
        {
            _rootNode->WalkElements(frustumPlanes, _center.cast<int32_t>(), InFunction, InContinuation, _extents, MaxDepthToWalk);
        }
    }

    void LooseOctree::WalkNodes(const std::function<bool(const AABBi&)>& InFunction, uint8_t MaxDepthToWalk)
    {
        if (_rootNode)
        {
            _rootNode->WalkNodes(InFunction, _center.cast<int32_t>(), _extents, MaxDepthToWalk);
        }
    }
    
    void LooseOctree::RemoveElement(IOctreeElement* InElement)
    {
        if (_rootNode)
        {
            auto octreeLink = InElement->GetOctreeLink();
            if (octreeLink)
            {
                const_cast<LooseOctreeNode*>(octreeLink)->RemoveElement(InElement);
            }
        }
    }

    void LooseOctree::Report()
    {
		std::vector<NodeInfo> nodes;
		nodes.resize(_maxDepth + 1);
		if (_rootNode)
		{
			_rootNode->Report(nodes);
		}

		int32_t TotalNodes = 0;
		int32_t TotalElements = 0;

		for (int32_t Iter = 0; Iter < nodes.size(); Iter++)
		{
            SPP_LOG(LOG_OCT, LOG_INFO, "Level: %d", Iter);
            SPP_LOG(LOG_OCT, LOG_INFO, " - Active Nodes Count: %d", nodes[Iter].activeNodes);
            SPP_LOG(LOG_OCT, LOG_INFO, " - Element Count: %d", nodes[Iter].elementCount);
            SPP_LOG(LOG_OCT, LOG_INFO, " - Extents: %d", GetExtentsAtDepth(Iter));

			TotalNodes += nodes[Iter].activeNodes;
			TotalElements += nodes[Iter].elementCount;
		}

        SPP_LOG(LOG_OCT, LOG_INFO, "TOTALS: ");
        SPP_LOG(LOG_OCT, LOG_INFO, " - Total Nodes: %d", TotalNodes);
        SPP_LOG(LOG_OCT, LOG_INFO, " - Total Elements: %d", TotalElements);
	}

    void LooseOctree::ImageGeneration(int32_t& oWidth, int32_t& oHeight, std::vector<Color3>& oData, const std::function<bool(const AABBi&)>& InFilter) const
    {        
        auto DesiredImageSize = powerOf2(2048);
        auto CurrentSize = powerOf2(_extents << 1);

        auto DeltaShift = std::max(0, CurrentSize - DesiredImageSize);

        oWidth = oHeight = (_extents << 1) >> DeltaShift;
        oData.resize(oWidth * oHeight, Color3(0, 0, 0));

        if (_rootNode)
        {
            _rootNode->ImageGeneration(this, oWidth, oHeight, _extents, oData, DeltaShift, { 0,0,0 }, InFilter);
        }
    }
}