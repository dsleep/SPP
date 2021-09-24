// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPOctree.h"

namespace SPP
{
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

    void LooseOctree::LooseOctreeNode::WalkElements(const Planed frustumPlanes[6],
        Vector3i InCurrentCenter,
        const std::function<bool(const IOctreeElement*)>& InFunction,
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
                 
                Vector3i childCenter = (InCurrentCenter + childCenterOffset);
                AABBi childAABB{ 
                    childCenter - Vector3i(looseChildExt,looseChildExt,looseChildExt),
                    childCenter + Vector3i(looseChildExt,looseChildExt,looseChildExt)
                     };
                
                if (boxInFrustum(frustumPlanes, childAABB))
                {
                    _children[ChildIter]->WalkElements(frustumPlanes, childCenter, InFunction, childExt, CurrentDepth - 1);
                }                
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
        inNodes[CurrentDepth].elementCount += _elements.size();

        for (int32_t ChildIter = 0; ChildIter < 8; ChildIter++)
        {
            if (_children[ChildIter])
            {
                _children[ChildIter]->Report(inNodes, CurrentDepth + 1);
            }
        }
    }

    void LooseOctree::LooseOctreeNode::AddElement(IOctreeElement* InElement)
    {
        _elements.push_back(InElement);
        InElement->SetOctreeLink(this);
    }

    void LooseOctree::LooseOctreeNode::RemoveElement(IOctreeElement* InElement)
    {
        SE_ASSERT(InElement->GetOctreeLink() == this);
        InElement->SetOctreeLink(nullptr);
        _elements.remove(InElement);        
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

    void LooseOctree::WalkElements(const Planed frustumPlanes[6], const std::function<bool(const IOctreeElement*)>& InFunction, uint8_t MaxDepthToWalk)
    {
        if (_rootNode)
        {
            _rootNode->WalkElements(frustumPlanes, _center.cast<int32_t>(), InFunction, _extents, MaxDepthToWalk);
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

    void LooseOctree::Report(std::ostream* io)
    {
        std::vector<NodeInfo> nodes;
        nodes.resize(_maxDepth + 1);
        if (_rootNode)
        {
            _rootNode->Report(nodes);
        }

        if (io)
        {
            (*io) << std::endl;

            int32_t TotalNodes = 0;
            int32_t TotalElements = 0;

            for (int32_t Iter = 0; Iter < nodes.size(); Iter++)
            {
                (*io) << "Level: " << Iter << std::endl;
                (*io) << " - Active Nodes Count: " << nodes[Iter].activeNodes << std::endl;
                (*io) << " - Element Count: " << nodes[Iter].elementCount << std::endl;

                TotalNodes += nodes[Iter].activeNodes;
                TotalElements += nodes[Iter].elementCount;
            }

            (*io) << "TOTALS: " << std::endl;
            (*io) << " - Total Nodes: " << TotalNodes << std::endl;
            (*io) << " - Total Elements: " << TotalElements << std::endl;

            (*io) << std::endl;
        }
    }

    void LooseOctree::ImageGeneration(int32_t& oWidth, int32_t& oHeight, std::vector<uint8_t>& oData) const
    {        
        auto DesiredImageSize = powerOf2(2048);
        auto CurrentSize = powerOf2(_extents << 1);

        auto DeltaShift = std::max(0, CurrentSize - DesiredImageSize);

        oWidth = oHeight = (_extents << 1) >> DeltaShift;
    }
}