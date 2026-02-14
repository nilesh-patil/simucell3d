#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <typeinfo>

#include "utils.hpp"
#include "contact_detection_strategy.hpp"
#include "custom_structures.hpp"

/**
 * Test Suite: Contact Detection Strategy Pattern
 *
 * Scientific Testing Philosophy:
 * - Validates factory pattern correctness for polymorphic strategy creation
 * - Verifies interface contract adherence (non-null returns, valid method calls)
 * - Tests boundary conditions (empty inputs, null queries)
 * - Ensures backward compatibility (USPG as default)
 * - Validates strategy type identity for algorithmic correctness
 *
 * Test Organization:
 * 1. Factory Creation Tests - Verify correct strategy instantiation
 * 2. Type Identity Tests - Ensure strategies are distinguishable
 * 3. Interface Contract Tests - Validate method signatures and basic behavior
 * 4. Boundary Condition Tests - Test edge cases and defensive programming
 */


//---------------------------------------------------------------------------------------------------------
// Test 1: Factory Creates USPG Strategy by Default
//
// Purpose: Verify that the factory correctly instantiates a USPG strategy when
//          ContactDetectionAlgorithm::USPG is specified.
//
// Scientific Rationale: USPG is the established baseline algorithm. The factory
//                       must reliably produce this strategy for backward compatibility.
//
// Success Criteria:
//   - Factory returns non-null pointer
//   - Strategy type is identifiable as USPG (via algorithm_type() method)
int test_factory_creates_uspg_by_default() {

    // Create minimal parameters structure for factory
    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    // Invoke factory method
    std::unique_ptr<contact_detection_strategy> strategy =
        contact_detection_strategy::create(params.contact_detection_algorithm_, params);

    // Verify non-null creation
    bool t1 = (strategy != nullptr);

    // Verify correct type identification
    bool t2 = (strategy->algorithm_type() == ContactDetectionAlgorithm::USPG);

    std::cout << "t1 (non-null USPG): " << t1 << std::endl;
    std::cout << "t2 (correct type):  " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Factory Creates Sweep-and-Prune Strategy for SWEEP_AND_PRUNE Enum
//
// Purpose: Verify that the factory correctly instantiates a SAP strategy when
//          ContactDetectionAlgorithm::SWEEP_AND_PRUNE is specified.
//
// Scientific Rationale: The factory must correctly dispatch to the new SAP algorithm.
//                       This is the primary functionality being added in Phase 2.
//
// Success Criteria:
//   - Factory returns non-null pointer
//   - Strategy type is identifiable as SWEEP_AND_PRUNE
int test_factory_creates_sap_for_sweep_and_prune() {

    // Create parameters with SAP algorithm specified
    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    // Invoke factory method
    std::unique_ptr<contact_detection_strategy> strategy =
        contact_detection_strategy::create(params.contact_detection_algorithm_, params);

    // Verify non-null creation
    bool t1 = (strategy != nullptr);

    // Verify correct type identification
    bool t2 = (strategy->algorithm_type() == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);

    std::cout << "t1 (non-null SAP): " << t1 << std::endl;
    std::cout << "t2 (correct type): " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Factory Returns Non-Null Pointer for All Enum Values
//
// Purpose: Verify that the factory never returns a null pointer for any valid
//          enum value, ensuring defensive programming.
//
// Scientific Rationale: Null pointers lead to undefined behavior and segfaults.
//                       The factory must guarantee valid object creation for all
//                       enum values to prevent runtime failures.
//
// Success Criteria:
//   - Both USPG and SWEEP_AND_PRUNE produce non-null pointers
int test_factory_returns_non_null_pointer() {

    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    // Create both strategy types
    std::unique_ptr<contact_detection_strategy> strategy_uspg =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> strategy_sap =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Verify both are non-null
    bool t1 = (strategy_uspg != nullptr);
    bool t2 = (strategy_sap != nullptr);

    std::cout << "t1 (USPG non-null): " << t1 << std::endl;
    std::cout << "t2 (SAP non-null):  " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Strategies Are Polymorphic and Support Interface Methods
//
// Purpose: Verify that strategies created by the factory can be used polymorphically
//          through the base interface, calling prepare() and get_candidate_faces().
//
// Scientific Rationale: The Strategy pattern requires that all implementations
//                       can be used interchangeably through the base interface.
//                       This test validates the polymorphic contract.
//
// Success Criteria:
//   - prepare() can be invoked without runtime errors
//   - get_candidate_faces() can be invoked without runtime errors
//   - Both methods work for USPG and SAP strategies
//
// Note: This test validates method invocability, not correctness (which requires
//       actual mesh data and will be tested in integration tests).
int test_strategies_are_polymorphic() {

    // Create both strategy types
    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    std::unique_ptr<contact_detection_strategy> strategy_uspg =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> strategy_sap =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Prepare empty data structures (boundary condition test)
    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> empty_aabbs;
    vec3 zero_bounds(0., 0., 0.);

    // Test that prepare() is callable on both strategies without crashing
    bool t1 = true;
    bool t2 = true;

    try {
        strategy_uspg->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);
    } catch (...) {
        t1 = false;
        std::cout << "USPG prepare() threw exception with empty input" << std::endl;
    }

    try {
        strategy_sap->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);
    } catch (...) {
        t2 = false;
        std::cout << "SAP prepare() threw exception with empty input" << std::endl;
    }

    // Test that get_candidate_faces() is callable (will return empty vector)
    bool t3 = true;
    bool t4 = true;

    try {
        vec3 query_pos(0., 0., 0.);
        cell* query_cell = nullptr;

        std::vector<face*> result_uspg = strategy_uspg->get_candidate_faces(query_pos, query_cell);
        t3 = true; // If we get here, no exception was thrown

    } catch (...) {
        t3 = false;
        std::cout << "USPG get_candidate_faces() threw exception" << std::endl;
    }

    try {
        vec3 query_pos(0., 0., 0.);
        cell* query_cell = nullptr;

        std::vector<face*> result_sap = strategy_sap->get_candidate_faces(query_pos, query_cell);
        t4 = true; // If we get here, no exception was thrown

    } catch (...) {
        t4 = false;
        std::cout << "SAP get_candidate_faces() threw exception" << std::endl;
    }

    std::cout << "t1 (USPG prepare callable):             " << t1 << std::endl;
    std::cout << "t2 (SAP prepare callable):              " << t2 << std::endl;
    std::cout << "t3 (USPG get_candidate_faces callable): " << t3 << std::endl;
    std::cout << "t4 (SAP get_candidate_faces callable):  " << t4 << std::endl;

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Prepare Accepts Empty Cell List Without Crashing
//
// Purpose: Verify defensive programming - empty input should not cause crashes
//          or undefined behavior.
//
// Scientific Rationale: Simulations may have edge cases where contact detection
//                       is invoked on empty or nearly-empty systems (e.g., during
//                       initialization, after cell death events). Robust algorithms
//                       must handle degenerate inputs gracefully.
//
// Success Criteria:
//   - prepare() completes without exceptions for empty inputs
//   - No segmentation faults or undefined behavior
//   - Both USPG and SAP strategies handle this gracefully
int test_prepare_accepts_empty_cell_list() {

    // Create both strategies
    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    std::unique_ptr<contact_detection_strategy> strategy_uspg =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> strategy_sap =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Create empty data structures
    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> empty_aabbs;
    vec3 zero_bounds(0., 0., 0.);

    // Test USPG
    bool t1 = true;
    try {
        strategy_uspg->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);
    } catch (const std::exception& e) {
        t1 = false;
        std::cout << "USPG threw exception on empty input: " << e.what() << std::endl;
    } catch (...) {
        t1 = false;
        std::cout << "USPG threw unknown exception on empty input" << std::endl;
    }

    // Test SAP
    bool t2 = true;
    try {
        strategy_sap->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);
    } catch (const std::exception& e) {
        t2 = false;
        std::cout << "SAP threw exception on empty input: " << e.what() << std::endl;
    } catch (...) {
        t2 = false;
        std::cout << "SAP threw unknown exception on empty input" << std::endl;
    }

    std::cout << "t1 (USPG handles empty): " << t1 << std::endl;
    std::cout << "t2 (SAP handles empty):  " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: get_candidate_faces Returns Valid Vector (May Be Empty)
//
// Purpose: Verify that get_candidate_faces() always returns a valid vector,
//          even when no candidates exist or input is degenerate.
//
// Scientific Rationale: The method contract specifies it returns a vector of
//                       face pointers. It must never return an invalid reference
//                       or cause undefined behavior. An empty vector is a valid
//                       result when no candidates exist.
//
// Success Criteria:
//   - Method returns without exceptions
//   - Returned vector is valid (even if empty)
//   - No undefined behavior for null query_cell or zero position
int test_get_candidate_faces_returns_vector() {

    // Create both strategies
    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    std::unique_ptr<contact_detection_strategy> strategy_uspg =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> strategy_sap =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Prepare with empty data
    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> empty_aabbs;
    vec3 zero_bounds(0., 0., 0.);

    strategy_uspg->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);
    strategy_sap->prepare(empty_cells, empty_faces, empty_aabbs, zero_bounds);

    // Query with degenerate inputs
    vec3 query_pos(0., 0., 0.);
    cell* null_cell = nullptr;

    // Test USPG
    bool t1 = true;
    bool t2 = true;
    try {
        std::vector<face*> result = strategy_uspg->get_candidate_faces(query_pos, null_cell);
        // Verify vector is valid (size() is safe to call on any valid vector)
        t2 = (result.size() == 0); // Should be empty for empty preparation
    } catch (...) {
        t1 = false;
        std::cout << "USPG get_candidate_faces threw exception" << std::endl;
    }

    // Test SAP
    bool t3 = true;
    bool t4 = true;
    try {
        std::vector<face*> result = strategy_sap->get_candidate_faces(query_pos, null_cell);
        // Verify vector is valid
        t4 = (result.size() == 0); // Should be empty for empty preparation
    } catch (...) {
        t3 = false;
        std::cout << "SAP get_candidate_faces threw exception" << std::endl;
    }

    std::cout << "t1 (USPG returns without exception): " << t1 << std::endl;
    std::cout << "t2 (USPG returns empty vector):      " << t2 << std::endl;
    std::cout << "t3 (SAP returns without exception):  " << t3 << std::endl;
    std::cout << "t4 (SAP returns empty vector):       " << t4 << std::endl;

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 7: Strategy Type Identity is Preserved After Creation
//
// Purpose: Verify that the strategy type can be queried and remains consistent
//          throughout the object's lifetime.
//
// Scientific Rationale: Type identity is essential for debugging, diagnostics,
//                       and performance analysis. The algorithm_type() method
//                       provides runtime type information for logging and validation.
//
// Success Criteria:
//   - algorithm_type() returns consistent value after creation
//   - Type identity matches the enum passed to factory
//   - Multiple calls to algorithm_type() return identical results
int test_strategy_type_identity_is_preserved() {

    // Create USPG strategy
    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;
    std::unique_ptr<contact_detection_strategy> strategy_uspg =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    // Verify USPG identity
    bool t1 = (strategy_uspg->algorithm_type() == ContactDetectionAlgorithm::USPG);
    bool t2 = (strategy_uspg->algorithm_type() == strategy_uspg->algorithm_type()); // Consistency

    // Create SAP strategy
    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    std::unique_ptr<contact_detection_strategy> strategy_sap =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Verify SAP identity
    bool t3 = (strategy_sap->algorithm_type() == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);
    bool t4 = (strategy_sap->algorithm_type() == strategy_sap->algorithm_type()); // Consistency

    // Verify strategies are distinguishable
    bool t5 = (strategy_uspg->algorithm_type() != strategy_sap->algorithm_type());

    std::cout << "t1 (USPG identity correct):     " << t1 << std::endl;
    std::cout << "t2 (USPG identity consistent):  " << t2 << std::endl;
    std::cout << "t3 (SAP identity correct):      " << t3 << std::endl;
    std::cout << "t4 (SAP identity consistent):   " << t4 << std::endl;
    std::cout << "t5 (strategies distinguishable):" << t5 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 8: Factory Handles Both Algorithm Types in Same Execution Context
//
// Purpose: Verify that multiple strategy instances can coexist and be created
//          in the same program execution without interference.
//
// Scientific Rationale: In complex simulations, multiple algorithms may be
//                       benchmarked or compared within the same run. The factory
//                       must support concurrent creation of different strategy types.
//
// Success Criteria:
//   - Multiple strategies can be created in sequence
//   - Each strategy maintains its own type identity
//   - No cross-contamination between strategy instances
int test_factory_handles_multiple_algorithm_types() {

    // Create parameters for both types
    global_simulation_parameters params_uspg;
    params_uspg.contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;

    global_simulation_parameters params_sap;
    params_sap.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;

    // Create multiple instances of each type
    std::unique_ptr<contact_detection_strategy> uspg1 =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> sap1 =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    std::unique_ptr<contact_detection_strategy> uspg2 =
        contact_detection_strategy::create(params_uspg.contact_detection_algorithm_, params_uspg);

    std::unique_ptr<contact_detection_strategy> sap2 =
        contact_detection_strategy::create(params_sap.contact_detection_algorithm_, params_sap);

    // Verify all are non-null
    bool t1 = (uspg1 != nullptr) && (sap1 != nullptr) &&
              (uspg2 != nullptr) && (sap2 != nullptr);

    // Verify type identities are correct
    bool t2 = (uspg1->algorithm_type() == ContactDetectionAlgorithm::USPG);
    bool t3 = (uspg2->algorithm_type() == ContactDetectionAlgorithm::USPG);
    bool t4 = (sap1->algorithm_type() == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);
    bool t5 = (sap2->algorithm_type() == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);

    // Verify instances are independent (different pointers)
    bool t6 = (uspg1.get() != uspg2.get());
    bool t7 = (sap1.get() != sap2.get());

    std::cout << "t1 (all non-null):         " << t1 << std::endl;
    std::cout << "t2 (uspg1 correct type):   " << t2 << std::endl;
    std::cout << "t3 (uspg2 correct type):   " << t3 << std::endl;
    std::cout << "t4 (sap1 correct type):    " << t4 << std::endl;
    std::cout << "t5 (sap2 correct type):    " << t5 << std::endl;
    std::cout << "t6 (uspg instances unique):" << t6 << std::endl;
    std::cout << "t7 (sap instances unique): " << t7 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main Test Runner
//
// Follows SimuCell3D test pattern: command-line argument selects test to run
int main(int argc, char** argv) {

    // Check command line format
    assert(argc == 2);

    // Get test name
    std::string test_name = argv[1];

    // Run selected test
    if (test_name == "test_factory_creates_uspg_by_default")
        return test_factory_creates_uspg_by_default();

    if (test_name == "test_factory_creates_sap_for_sweep_and_prune")
        return test_factory_creates_sap_for_sweep_and_prune();

    if (test_name == "test_factory_returns_non_null_pointer")
        return test_factory_returns_non_null_pointer();

    if (test_name == "test_strategies_are_polymorphic")
        return test_strategies_are_polymorphic();

    if (test_name == "test_prepare_accepts_empty_cell_list")
        return test_prepare_accepts_empty_cell_list();

    if (test_name == "test_get_candidate_faces_returns_vector")
        return test_get_candidate_faces_returns_vector();

    if (test_name == "test_strategy_type_identity_is_preserved")
        return test_strategy_type_identity_is_preserved();

    if (test_name == "test_factory_handles_multiple_algorithm_types")
        return test_factory_handles_multiple_algorithm_types();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
