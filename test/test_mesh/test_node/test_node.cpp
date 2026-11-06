#include "node_tester.hpp"


/*

Test the method of the node class

*/


typedef std::shared_ptr<cell> cell_ptr;

//---------------------------------------------------------------------------------------------------------
//Test the default constructors of the node class
int node_tester::default_constructors_test(){


    //Check the default copy constructor

    //Create a dummy face
    node n1(1., 2., 3., 0);

    //This copy has the same cell_ptr but is not registered in the cell as one of the faces
    node n2(n1);
    auto [coord_1, coord_2, coord_3 ] = n2.pos().to_array();
    bool t2 = coord_1 == 1. &&  coord_2 == 2. && coord_3 == 3.;
    bool t3 = n1.get_local_id() == n2.get_local_id();

    //Do the same test with the copy assignment function
    node n3 =  n1;
    auto [coord_4, coord_5, coord_6 ] = n3.pos().to_array();
    bool t4 = coord_4 == 1. &&  coord_5 == 2. && coord_6 == 3.;
    bool t5 = n1.get_local_id() == n3.get_local_id();
    return !(t2 && t3 && t4 && t5);
}

//---------------------------------------------------------------------------------------------------------
//Test the constructor of the node class
int node_tester::trivial_instantiation_test(){

    //Generate a node with default coordinates 
    node n1(1);

    //Check the id of the node 
    bool t1 = n1.get_local_id() == 1;

    //Check the position of the node
    bool t2 = (n1.pos().dx() == 0. && n1.pos().dy() == 0. && n1.pos().dz() == 0.);

    //Create a node with arbitrary coordinates
    node n2(1., 2. , 3., 2);

    //Check the id of the node 
    bool t3 = n2.get_local_id() == 2;

    //Check the position of the node
    bool t4 = (n2.pos().dx() == 1. && n2.pos().dy() == 2. && n2.pos().dz() == 3.);

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
//Test the reset of a node
int node_tester::node_reset_test(){

    //Generate a node with default coordinates 
    node n0(0), n1(1), n2(2), n3(3);
    std::vector<node> node_lst = {n0, n1, n2, n3};
    
    face f1(n0, n1, n2, 0);
    std::vector<face> face_lst{f1};

    //Create a dummy cell
    cell_ptr c1 = std::make_shared<cell>(node_lst, face_lst, 1);

    //Reset the first and third node of the cell
    c1->node_lst_[0].reset();
    c1->node_lst_[3].reset();

    //Check that these two nodes are not used anymore by the cel
    bool t1 = c1->node_lst_[0].is_used_ == false;
    bool t2 = c1->node_lst_[3].is_used_ == false;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int node_tester::reset_local_id_test(){

    //Generate a node with default coordinates 
    node n1(1);

    //Reset its id
    n1.set_local_id(0);

    return !(n1.get_local_id() == 0);
}

//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int node_tester::set_is_used_test(){

    //Generate a node with default coordinates 
    node n1(1);

    bool t1 = n1.is_used() == true;

    n1.set_is_used(false);

    bool t2 = n1.is_used() == false;

    return !(t1 && t2);
}

//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int node_tester::operator_minus_test(){

    node n1(0., 0., 0., 1);
    node n2(1., 1., 1., 2);
    vec3 v = n2 - n1;

    return !(v == vec3(1, 1, 1));
}

//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
//The coupling setter is a compare-and-set: a farther partner never replaces a closer one on the same cell
int node_tester::coupling_compare_and_set_test(){
#if CONTACT_MODEL_INDEX == 2
    node n(0., 0., 0., 0);
    bool ok = true;

    //No partner yet: the first candidate is stored
    ok &= n.set_coupled_node_and_min_distance(7, 3, 1.0);
    ok &= n.get_coupled_squared_distance(7) == 1.0;

    //A farther candidate on the same cell is refused and leaves the closer partner in place
    ok &= !n.set_coupled_node_and_min_distance(7, 4, 2.0);
    ok &= n.coupled_nodes_map_.at(7) == std::make_pair(3u, 1.0);

    //A closer candidate replaces it
    ok &= n.set_coupled_node_and_min_distance(7, 5, 0.5);
    ok &= n.coupled_nodes_map_.at(7) == std::make_pair(5u, 0.5);

    //A candidate at the same distance is not farther: it is stored (this keeps the single-thread behaviour of the
    //contact model, which only excluded a candidate when the existing partner was strictly closer)
    ok &= n.set_coupled_node_and_min_distance(7, 6, 0.5);
    ok &= n.coupled_nodes_map_.at(7) == std::make_pair(6u, 0.5);

    //Partners on different cells are independent
    ok &= n.set_coupled_node_and_min_distance(8, 1, 10.0);
    ok &= n.get_nb_coupled_nodes() == 2;
    ok &= n.coupled_nodes_map_.at(7) == std::make_pair(6u, 0.5);

    //No partner on a cell reads as "infinitely far"
    ok &= n.get_coupled_squared_distance(9) == std::numeric_limits<double>::max();

    return !ok;
#else
    return 0;
#endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
//couple_if_closest() stores both directions of a pair or neither
int node_tester::couple_if_closest_test(){
#if CONTACT_MODEL_INDEX == 2
    //a and e belong to the cell 0, b and c to the cell 1
    node a(0., 0., 0., 10), e(0., 0., 0., 11), b(0., 0., 0., 20), c(0., 0., 0., 21);
    bool ok = true;

    ok &= node::couple_if_closest(a, 0, b, 1, 1.0);
    ok &= a.coupled_nodes_map_.at(1) == std::make_pair(20u, 1.0);
    ok &= b.coupled_nodes_map_.at(0) == std::make_pair(10u, 1.0);

    //a already has a closer partner on the cell 1: refused, and c is left untouched
    ok &= !node::couple_if_closest(a, 0, c, 1, 2.0);
    ok &= a.coupled_nodes_map_.at(1) == std::make_pair(20u, 1.0);
    ok &= !c.is_coupled();

    //b already has a closer partner on the cell 0: refused, and e is left untouched
    ok &= !node::couple_if_closest(e, 0, b, 1, 3.0);
    ok &= b.coupled_nodes_map_.at(0) == std::make_pair(10u, 1.0);
    ok &= !e.is_coupled();

    //A closer pair replaces the partners of both nodes
    ok &= node::couple_if_closest(e, 0, b, 1, 0.25);
    ok &= b.coupled_nodes_map_.at(0) == std::make_pair(11u, 0.25);
    ok &= e.coupled_nodes_map_.at(1) == std::make_pair(20u, 0.25);

    return !ok;
#else
    return 0;
#endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
//Under contention the closest candidate always wins, whatever the order in which the threads arrive
int node_tester::concurrent_coupling_test(){
#if CONTACT_MODEL_INDEX == 2
    constexpr int nb_threads = 8;
    constexpr unsigned nb_candidates = 4096;
    bool ok = true;

    for(unsigned trial = 0; trial < 20; trial++){

        //Candidate k of the cell 1 is at the squared distance dist[k]; distinct values, shuffled
        std::vector<double> dist(nb_candidates);
        std::iota(dist.begin(), dist.end(), 1.0);
        std::mt19937 gen(trial);
        std::shuffle(dist.begin(), dist.end(), gen);
        const unsigned best = static_cast<unsigned>(std::min_element(dist.begin(), dist.end()) - dist.begin());

        //Setter on one node
        node target(0., 0., 0., 0);
        #pragma omp parallel for num_threads(nb_threads) schedule(dynamic, 16)
        for(unsigned k = 0; k < nb_candidates; k++){
            target.set_coupled_node_and_min_distance(1, k, dist[k]);
        }
        ok &= target.coupled_nodes_map_.at(1) == std::make_pair(best, dist[best]);

        //Pair transactions, with the two nodes passed in both orders to exercise the deadlock-free locking
        node hub(0., 0., 0., 0);
        std::vector<node> candidates;
        candidates.reserve(nb_candidates);
        for(unsigned k = 0; k < nb_candidates; k++) candidates.emplace_back(0., 0., 0., k);

        #pragma omp parallel for num_threads(nb_threads) schedule(dynamic, 16)
        for(unsigned k = 0; k < nb_candidates; k++){
            if(k % 2) node::couple_if_closest(hub, 0, candidates[k], 1, dist[k]);
            else      node::couple_if_closest(candidates[k], 1, hub, 0, dist[k]);
        }
        ok &= hub.coupled_nodes_map_.at(1) == std::make_pair(best, dist[best]);
        ok &= candidates[best].coupled_nodes_map_.at(0) == std::make_pair(0u, dist[best]);
    }

    return !ok;
#else
    return 0;
#endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
//add_force_atomic() does not lose updates when several threads add forces to the same node
int node_tester::atomic_force_test(){
    constexpr int nb_threads = 8;
    constexpr long nb_adds = 400000;
    node n(0., 0., 0., 0);

    #pragma omp parallel for num_threads(nb_threads) schedule(static, 1)
    for(long k = 0; k < nb_adds; k++){
        n.add_force_atomic(vec3(1., 2., -3.));
    }

    //Integers below 2^53 are exact in double precision, so the sum must be exact
    const vec3& f = n.force();
    return !(f.dx() == nb_adds && f.dy() == 2. * nb_adds && f.dz() == -3. * nb_adds);
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// The main function
int main (int argc, char** argv){

    //Check that the command line input is correctly formatted
    assert(argc == 2); 

    //Get the name of the test to run
    std::string test_name = argv[1];

    node_tester tester;
    
    //Run the selected test
    if (test_name == "default_constructors_test")         return tester.default_constructors_test();
    if (test_name == "trivial_instantiation_test")        return tester.trivial_instantiation_test();
    if (test_name == "node_reset_test")                   return tester.node_reset_test();
    if (test_name == "reset_local_id_test")                     return tester.reset_local_id_test();
    if (test_name == "set_is_used_test")                  return tester.set_is_used_test();
    if (test_name == "operator_minus_test")               return tester.operator_minus_test();
    if (test_name == "coupling_compare_and_set_test")     return tester.coupling_compare_and_set_test();
    if (test_name == "couple_if_closest_test")            return tester.couple_if_closest_test();
    if (test_name == "concurrent_coupling_test")          return tester.concurrent_coupling_test();
    if (test_name == "atomic_force_test")                 return tester.atomic_force_test();




    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------

