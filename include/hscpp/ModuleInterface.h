#pragma once

#include <unordered_map>
#include <vector>
#include <assert.h>

#include "hscpp/ModuleSharedState.h"
#include "hscpp/Constructors.h"
#include "hscpp/ITracker.h"

#define HSCPP_API __declspec(dllexport)

namespace hscpp
{
    // Interface into the module. Functions are marked 'virtual' to force using the new module's
    // methods. Otherwise, symbol resolution will be ambiguous, and the main executable will
    // continue to use its own implementation.
    class ModuleInterface
    {
    public:
        virtual void SetTrackersByKey(
            std::unordered_map<std::string, std::vector<ITracker*>>* pTrackersByKey)
        {
            ModuleSharedState::s_pTrackersByKey = pTrackersByKey;
        }

        virtual void SetAllocator(IAllocator* pAllocator)
        {
            ModuleSharedState::s_pAllocator = pAllocator;
        }

        virtual void PerformRuntimeSwap()
        {
            // Get constructors registered within this module.
            auto& constructorsByKey = Constructors::GetConstructorsByKey();

            for (const auto& constructorPair : constructorsByKey)
            {
                // Find tracked objects corresponding to this constructor. If not found, this must
                // be a new class, so no instances have been created yet.
                std::string key = constructorPair.first;

                auto trackedObjectsPair = ModuleSharedState::s_pTrackersByKey->find(key);
                if (trackedObjectsPair != ModuleSharedState::s_pTrackersByKey->end())
                {
                    // Get tracked objects, and make a copy. As objects are freed, their tracker
                    // will be erased from the trackedObjects vector.
                    std::vector<ITracker*>& trackedObjects = trackedObjectsPair->second;
                    std::vector<ITracker*> oldTrackedObjects = trackedObjects;

                    size_t nInstances = trackedObjects.size();

                    std::vector<SwapInfo> swapInfos(nInstances);
                    std::vector<uint64_t> memoryIds(nInstances);

                    // Free the old objects; they will be swapped out with new instances.
                    for (size_t i = 0; i < nInstances; ++i)
                    {
                        swapInfos.at(i).m_Id = i;
                        swapInfos.at(i).m_Phase = SwapPhase::BeforeSwap;

                        ITracker* pTracker = oldTrackedObjects.at(i);

                        pTracker->CallSwapHandler(swapInfos.at(i));
                        memoryIds.at(i) = pTracker->FreeTrackedObject();
                    }

                    // Freeing the tracked objects should have also deleted their tracker, so this
                    // list should now be empty.
                    assert(trackedObjects.empty());

                    // Create new instances from the new constructors. These will automatically
                    // register themselves into the m_pTrackersByKey map.
                    IConstructor* pConstructor = Constructors::GetConstructor(key);

                    for (size_t i = 0; i < nInstances; ++i)
                    {
                        pConstructor->Construct(memoryIds.at(i));

                        // After construction, a new tracker should have been added to trackedObjects.
                        ITracker* pTracker = trackedObjects.at(i);

                        swapInfos.at(i).m_Phase = SwapPhase::AfterSwap;
                        pTracker->CallSwapHandler(swapInfos.at(i));
                    }
                }
            }
        }
    };
}

extern "C"
{
    HSCPP_API inline hscpp::ModuleInterface* Hscpp_GetModuleInterface()
    {
        static hscpp::ModuleInterface moduleInterface;
        return &moduleInterface;
    }
}