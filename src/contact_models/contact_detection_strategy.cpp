#include "contact_detection_strategy.hpp"
#include "contact_detection_uspg_strategy.hpp"
#include "contact_detection_sap_strategy.hpp"


//---------------------------------------------------------------------------------------------
// Factory method to create contact detection strategy instances
//---------------------------------------------------------------------------------------------
std::unique_ptr<contact_detection_strategy> contact_detection_strategy::create(
    ContactDetectionAlgorithm algorithm,
    const global_simulation_parameters& params
) {
    switch (algorithm) {
        case ContactDetectionAlgorithm::USPG:
            return std::make_unique<contact_detection_uspg_strategy>(params);

        case ContactDetectionAlgorithm::SWEEP_AND_PRUNE:
            return std::make_unique<contact_detection_sap_strategy>(params);

        default:
            // Default to USPG for backward compatibility and safety
            // This handles any future enum additions gracefully
            return std::make_unique<contact_detection_uspg_strategy>(params);
    }
}
//---------------------------------------------------------------------------------------------
