#include "local_mesh_refiner_tester.hpp"
#include <map>
#include <set>
#include <memory>
#include <iostream>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <new>
#include <omp.h>
#include <optional>
#include <stdexcept>





//Unit tests for the local_mesh_refiner class

int local_mesh_refiner_tester::can_be_merged_test(){
    
//          A
//       / /|\ ＼     
//     /  / | \  ＼    
//   /  6/  |  \ 3 ＼  
//  F__E/ 1 | 2 \D__＼C
//  \   \   |   /   ／
//    \5 \  |  / 4／  
//      \ \ | / ／    
//       \ \|/／      
//          B
//   


   //Create the geometry shown above

    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,   //Node A 0
        0., -2., 0.,   //Node B 1 
        2., -1., 0.,   //Node C 2
        1., -1., 0.,   //Node D 3
       -1., -1., 0.,   //Node E 4
       -2., -1., 0.,   //Node F 5

        0.,  0., -1.,   //Node G 6
        0., -2., -1.,   //Node H 7 
        2., -1., -1.,   //Node I 8
       -2., -1., -1.,   //Node J 9

    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{
        {0, 1, 4},  //Face ABE 1
        {0, 1, 3},  //Face ABD 2
        {0, 3, 2},  //Face ADC 3
        {1, 2, 3},  //Face BCD 4
        {1, 4, 5},  //Face BEF 5
        {0, 4, 5},  //Face AEF 6

        {2, 6, 8},  //Face AGI 7
        {0, 6, 2},  //Face ACI 8
        {2, 8, 7},  //Face BHI 9
        {1, 2, 7},  //Face BCI 10

        {5, 9, 6},  //Face AFJ 11
        {0, 5, 6},  //Face AGJ 12
        {1, 7, 5},  //Face BHJ 13
        {5, 7, 9},  //Face BFJ 14

        {6, 7, 8},  //Face GHI 14
        {6, 7, 9}   //Face GHJ 14
    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;

    //Write the created mesh to check it
    mesh_writer::write_cell_data_file(std::string("./test_1.vtk"), {m});

    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();

    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge& edge_ab = const_cast<edge&>(*edge_it);

    bool t1 = edge_ab.is_manifold() == true;

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);


    //Remove the face ABD ADC and BCD
    bool t2 = lmr.can_be_merged(edge_ab, c) == false;


    std::cout << t1 << std::endl;
    std::cout << t2 << std::endl;
 
    return !(t1 && t2);
}




/*
int local_mesh_refiner_tester::prepare_merger_test_1(){
    
//          A
//       / /|\ ＼     
//     /  / | \  ＼    
//   /  6/  |  \ 3 ＼  
//  F__E/ 1 | 2 \D__＼C
//  \   \   |   /   ／
//    \5 \  |  / 4／  
//      \ \ | / ／    
//       \ \|/／      
//          B
//   


   //Create the geometry shown above

    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,   //Node A 0
        0., -2., 0.,   //Node B 1 
        2., -1., 0.,   //Node C 2
        1., -1., 0.,   //Node D 3
       -1., -1., 0.,   //Node E 4
       -2., -1., 0.,   //Node F 5

        0.,  0., -1.,   //Node G 6
        0., -2., -1.,   //Node H 7 
        2., -1., -1.,   //Node I 8
       -2., -1., -1.,   //Node J 9

    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{
        {0, 1, 4},  //Face ABE 1
        {0, 1, 3},  //Face ABD 2
        {0, 3, 2},  //Face ADC 3
        {1, 2, 3},  //Face BCD 4
        {1, 4, 5},  //Face BEF 5
        {0, 4, 5},  //Face AEF 6

        {2, 6, 8},  //Face AGI 7
        {0, 6, 2},  //Face ACI 8
        {2, 8, 7},  //Face BHI 9
        {1, 2, 7},  //Face BCI 10

        {5, 9, 6},  //Face AFJ 11
        {0, 5, 6},  //Face AGJ 12
        {1, 7, 5},  //Face BHJ 13
        {5, 7, 9},  //Face BFJ 14

        {6, 7, 8},  //Face GHI 14
        {6, 7, 9}   //Face GHJ 14
    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;

    //Write the created mesh to check it
    mesh_writer::write_cell_data_file(std::string("./test_1.vtk"), {m});

    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();

    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge& edge_ab = const_cast<edge&>(*edge_it);

    bool t1 = edge_ab.is_manifold() == true;

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);

    edge_set dummy_set;

    //Remove the face ABD ADC and BCD
    lmr.prepare_merger(edge_ab, c, dummy_set);
 
    bool t2 = c->free_face_queue_.size() == 4;

    //Get the 2 faces that should be left
    const face& f1 = c->get_face_lst()[edge_it->f1()];
    const face& f2 = c->get_face_lst()[edge_it->f2()];

    //Make sure the 2 faces are made of the correct nodes
    std::array<unsigned, 3> f1_nodes = f1.get_node_ids();
    std::array<unsigned, 3> f2_nodes = f2.get_node_ids();

    //Sort the nodes 
    std::sort(f1_nodes.begin(), f1_nodes.end());
    std::sort(f2_nodes.begin(), f2_nodes.end());

    print_container(f1_nodes);
    print_container(f2_nodes);

    bool t3 = (
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{0, 1, 2}.begin()) ||
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{0, 1, 5}.begin())
            );
    bool t4 = (
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{0, 1, 2}.begin()) ||
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{0, 1, 5}.begin())
            );

    //Write the result in a vtk file
    mesh_writer::write_cell_data_file(std::string("./test_2.vtk"), {c});

    std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;
    std::cout << "t3 " << t3 << std::endl;
    std::cout << "t4 " << t4 << std::endl;

    return !(t1 && t2 && t3 && t4);
}




int local_mesh_refiner_tester::prepare_merger_test_2(){
//   
//          A
//       / /|\ ＼     
//     /  / | \  ＼    
//   /  6/  |  \ 3 ＼  
//  F__E/ 1 | 2 \D__＼C
//  \   \   |   /   ／
//    \5 \  |  / 4／  
//      \ \ | / ／    
//       \ \|/／      
//          B
//   

   //Create the geometry shown above

    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,   //Node A 0
        0., -2., 0.,   //Node B 1 
        2., -1., 0.,   //Node C 2
        1., -1., 0.,   //Node D 3
       -1., -1., 0.,   //Node E 4
       -2., -1., 0.,   //Node F 5

        0.,  0., -1.,   //Node G 6
        0., -2., -1.,   //Node H 7 
        2., -1., -1.,   //Node I 8
       -2., -1., -1.,   //Node J 9

       -1.5, -1., 0.,   //Node K 10
        1.5, -1., 0.,   //Node L 11


    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{
        {0, 1, 4},  //Face ABE 1
        {0, 1, 3},  //Face ABD 2

        {0, 11, 2},  //Face ALC 3
        {1, 2, 11},  //Face BCL 4
        {0, 3, 11},  //Face ADL 3
        {1, 11, 3},  //Face BLD 4

        {1, 10, 5},  //Face BEF 5
        {0, 10, 5},  //Face AEF 6
        {0, 4, 10},  //Face AEK 6
        {1, 4, 10},  //Face BEK 6


        {2, 6, 8},  //Face AGI 7
        {0, 6, 2},  //Face ACI 8
        {2, 8, 7},  //Face BHI 9
        {1, 2, 7},  //Face BCI 10

        {5, 9, 6},  //Face AFJ 11
        {0, 5, 6},  //Face AGJ 12
        {1, 7, 5},  //Face BHJ 13
        {5, 7, 9},  //Face BFJ 14

        {6, 7, 8},  //Face GHI 14
        {6, 7, 9}   //Face GHJ 14
    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;

    //Write the created mesh to check it
    mesh_writer::write_cell_data_file(std::string("./test_3.vtk"), {m});

    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();

    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge& edge_ab = const_cast<edge&>(*edge_it);

    bool t1 = edge_ab.is_manifold() == true;

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);

    edge_set dummy_set;

    //Remove the face ABD ADC and BCD
    lmr.prepare_merger(edge_ab, c, dummy_set);
 
    bool t2 = c->free_face_queue_.size() == 8;

    //Get the 2 faces that should be left
    const face& f1 = c->get_face_lst()[edge_it->f1()];
    const face& f2 = c->get_face_lst()[edge_it->f2()];

    //Make sure the 2 faces are made of the correct nodes
    std::array<unsigned, 3> f1_nodes = f1.get_node_ids();
    std::array<unsigned, 3> f2_nodes = f2.get_node_ids();

    //Sort the nodes 
    std::sort(f1_nodes.begin(), f1_nodes.end());
    std::sort(f2_nodes.begin(), f2_nodes.end());

    print_container(f1_nodes);
    print_container(f2_nodes);

    bool t3 = (
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{0, 1, 2}.begin()) ||
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{0, 1, 5}.begin())
            );
    bool t4 = (
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{0, 1, 2}.begin()) ||
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{0, 1, 5}.begin())
            );

    //Write the result in a vtk file
    mesh_writer::write_cell_data_file(std::string("./test_4.vtk"), {c});

    std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;
    std::cout << "t3 " << t3 << std::endl;
    std::cout << "t4 " << t4 << std::endl;

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------
*/

//---------------------------------------------------------------------------------------------------------
int local_mesh_refiner_tester::edge_swap_test(){


    /*
            A
           /|\     
          / | \     
         /  |  \   
       C/ 1 | 2 \D
        \   |   /   
         \  |  /   
          \ | /     
           \|/      
            B
    */

   //Create the geometry shown above

    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,    //Node A 0
        0., -3., 0.,    //Node B 1 
       -1., -1.5, 0.,   //Node C 2
        1., -1.5, 0.,   //Node D 3

        0.,  0.,  -1.,   //Node E 4
        0., -3.,  -1.,   //Node F 5 
       -1., -1.5, -1.,   //Node G 6
        1., -1.5, -1.,   //Node H 7
    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{
        {0, 1, 2},  //Face ABC 
        {0, 1, 3},   //Face ABD 
        {0, 4, 2},   //Face AEC 
        {2, 6, 4},   //Face CGE 
        {2, 6, 5},   //Face CGF 
        {5, 1, 2},    //Face FBC
        {0, 3, 7},    //Face ADH
        {7, 4, 0},    //Face HEA
        {3, 1, 5},    //Face DBF
        {5, 7, 3},    //Face FHD
        {4, 5, 6},    //Face EFG
        {4, 5, 7},    //Face EFH

    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;

    //Write the created mesh to check it
    mesh_writer::write_cell_data_file(std::string("./test_5.vtk"), {m});

    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();

    
    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge& edge_ab = const_cast<edge&>(*edge_it);

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);

    //Remove the face ABD ADC and BCD
    lmr.swap_edge(edge_ab, c);

    mesh_writer::write_cell_data_file(std::string("./test_6.vtk"), {c});

    bool t1 = c->edge_set_.find(edge(0, 1)) == c->edge_set_.end();

    auto edge_it2 = c->edge_set_.find(edge(2, 3));

    if(edge_it2 == c->edge_set_.end()){
        std::cout << "edge cd not found" << std::endl;
        return 1;
    }

    const edge& edge_cd = *(edge_it2);

    const face& f1 = c->get_face_lst()[edge_cd.f1()];
    const face& f2 = c->get_face_lst()[edge_cd.f2()];

    //Make sure the 2 faces are made of the correct nodes
    std::array<unsigned, 3> f1_nodes = f1.get_node_ids();
    std::array<unsigned, 3> f2_nodes = f2.get_node_ids();

    //Sort the nodes
    std::sort(f1_nodes.begin(), f1_nodes.end());
    std::sort(f2_nodes.begin(), f2_nodes.end());

    bool t2 = (
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{0, 2, 3}.begin()) ||
                std::equal(f1_nodes.begin(), f1_nodes.end(), std::vector<unsigned int>{1, 2, 3}.begin())
            );

    bool t3 = (
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{0, 2, 3}.begin()) ||
                std::equal(f2_nodes.begin(), f2_nodes.end(), std::vector<unsigned int>{1, 2, 3}.begin())
            );


    std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;
    std::cout << "t3 " << t3 << std::endl;

    return !(t1 && t2 && t3);


}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int local_mesh_refiner_tester::split_edge_test(){
    /*

    Start by recreating the geometry shown below

       E____C___F
       |   /\   |  
       |  /  \  |   
       | /    \ |  
       A/______\B
       |\      /|   
       | \    / |  
       |  \  /  |   
       |___\/___|   
       H   D    G
    */


    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,    //Node A 0
        1.,  0., 0.,    //Node B 1
        0.5, 1., 0.,    //Node C 2
        0.5, -1., 0.,   //Node D 3
        0.,  1.,  0.,   //Node E 4      
        1.,  1.,  0.,   //Node F 5
        1., -1., 0.,    //Node G 6
        0., -1., 0.,    //Node H 7


        0.,  0., -1.,    //Node I 8
        1.,  0., -1.,    //Node J 9
        0.5, 1., -1.,    //Node K 10
        0.5, -1., -1.,   //Node L 11
        0.,  1.,  -1.,   //Node M 12      
        1.,  1.,  -1.,   //Node N 13
        1., -1., -1.,    //Node O 14
        0., -1., -1.,    //Node P 15
    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{

        //The top 
        {0, 1, 2},  //Face ABC 0
        {0, 1, 3},  //Face ABD 1
        {0, 2, 4},  //Face ACE 2
        {1, 2, 5},  //Face BCF 3
        {0, 3, 7},  //Face ADH 4
        {1, 6, 3},  //Face BGD 5

        //The bottom
        {8, 9, 10},  //Face IJK 6
        {8, 9, 11},  //Face IJL 7
        {8, 10, 12},  //Face IKM 8
        {9, 10, 13},  //Face JKN 9
        {8, 11, 15},  //Face IPL 10
        {9, 14, 11},  //Face JLO 11

        //The sides
        {0, 8, 4 }, // Face AIE 
        {4, 12, 8}, // Face EMI
        {0, 8, 15}, // Face AIP
        {15, 7, 0}, // Face PHA

        {1, 9, 5 }, // Face BJF 
        {5, 13, 9}, // Face FNJ
        {1, 9, 14}, // Face BJO
        {14, 6, 1}, // Face OGB

        {4, 2, 10 }, // Face ECK
        {10, 12, 4}, // Face KME
        {5, 2, 10 }, // Face FCK
        {10, 13, 5}, // Face KNF

        {7, 3, 11 }, // Face HDL
        {11, 15, 7}, // Face LPH
        {6, 3, 11 }, // Face GDL
        {11, 14, 6}, // Face LOG

    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;

    //Write the created mesh to check it
    mesh_writer::write_cell_data_file(std::string("./test_7.vtk"), {m});


    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();


    #if DYNAMIC_MODEL_INDEX == 0
    //Add somee momentum to the nodes 0 and 1
    c->node_lst_[0].set_momentum(vec3(0, 0, 1));
    c->node_lst_[1].set_momentum(vec3(0, 0, 1));
    #endif


    const unsigned nb_edge_before = c->get_edge_set().size();
    const unsigned nb_node_before = c->get_node_lst().size();
    const unsigned nb_face_before = c->get_face_lst().size();

    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge& edge_ab = const_cast<edge&>(*edge_it);

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);

    edge_set dummy_set;

    //Remove the face ABD ADC and BCD
    lmr.split_edge(edge_ab, c, dummy_set);

    mesh_writer::write_cell_data_file(std::string("./test_8.vtk"), {c});

    //Check that the new edges have been added to the edge to check
    bool t1 = dummy_set.size() == 4;

    //Make sure the number of edges in the cell has increased by 3
    bool t2 = c->get_edge_set().size() == nb_edge_before + 3;

    unsigned nb_node_after = std::count_if(c->get_node_lst().begin(), c->get_node_lst().end(), [](const node& n){return n.is_used();});
    unsigned nb_face_after = std::count_if(c->get_face_lst().begin(), c->get_face_lst().end(), [](const face& f){return f.is_used();});

    bool t3 = nb_node_after == nb_node_before + 1;
    bool t4 = nb_face_after == nb_face_before + 2;
 
    //Make sure all the edges of the cell are connected to 2 faces
    bool t5 = c->is_manifold();

    //Make sure no momentum has been lost
    #if DYNAMIC_MODEL_INDEX == 0
        const vec3 total_momentum = std::accumulate(
            c->get_node_lst().begin(), 
            c->get_node_lst().end(), 
            vec3(0, 0, 0), 
            [](const vec3& a, const node& b){return a + b.momentum();}
        );
        bool t6 = total_momentum == vec3(0, 0, 2);
    #else
        bool t6 = true;
    #endif
    

    std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;
    std::cout << "t3 " << t3 << std::endl;
    std::cout << "t4 " << t4 << std::endl;
    std::cout << "t5 " << t5 << std::endl;
    std::cout << "t6 " << t6 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
int local_mesh_refiner_tester::merge_edge_test(){
    /*

    Start by recreating the geometry shown below

       E____C___F
       |   /\   |  
       |  /  \  |   
       | /    \ |  
       A/______\B
       |\      /|   
       | \    / |  
       |  \  /  |   
       |___\/___|   
       H   D    G
    */


    //Create the nodes
    std::vector<double> node_pos_lst{
        0.,  0., 0.,    //Node A 0
        1.,  0., 0.,    //Node B 1
        0.5, 1., 0.,    //Node C 2
        0.5, -1., 0.,   //Node D 3
        0.,  1.,  0.,   //Node E 4      
        1.,  1.,  0.,   //Node F 5
        1., -1., 0.,    //Node G 6
        0., -1., 0.,    //Node H 7


        0.,  0., -1.,    //Node I 8
        1.,  0., -1.,    //Node J 9
        0.5, 1., -1.,    //Node K 10
        0.5, -1., -1.,   //Node L 11
        0.,  1.,  -1.,   //Node M 12      
        1.,  1.,  -1.,   //Node N 13
        1., -1., -1.,    //Node O 14
        0., -1., -1.,    //Node P 15
    };

    //Create the faces
    std::vector<std::vector<unsigned>> face_conn_lst{

        //The top 
        {0, 1, 2},  //Face ABC 0
        {0, 1, 3},  //Face ABD 1
        {0, 2, 4},  //Face ACE 2
        {1, 2, 5},  //Face BCF 3
        {0, 3, 7},  //Face ADH 4
        {1, 6, 3},  //Face BGD 5

        //The bottom
        {8, 9, 10},  //Face IJK 6
        {8, 9, 11},  //Face IJL 7
        {8, 10, 12},  //Face IKM 8
        {9, 10, 13},  //Face JKN 9
        {8, 11, 15},  //Face IPL 10
        {9, 14, 11},  //Face JLO 11

        //The sides
        {0, 8, 4 }, // Face AIE 
        {4, 12, 8}, // Face EMI
        {0, 8, 15}, // Face AIP
        {15, 7, 0}, // Face PHA

        {1, 9, 5 }, // Face BJF 
        {5, 13, 9}, // Face FNJ
        {1, 9, 14}, // Face BJO
        {14, 6, 1}, // Face OGB

        {4, 2, 10 }, // Face ECK
        {10, 12, 4}, // Face KME
        {5, 2, 10 }, // Face FCK
        {10, 13, 5}, // Face KNF

        {7, 3, 11 }, // Face HDL
        {11, 15, 7}, // Face LPH
        {6, 3, 11 }, // Face GDL
        {11, 14, 6}, // Face LOG

    };

    mesh m;
    m.node_pos_lst = node_pos_lst;
    m.face_point_ids = face_conn_lst;


    //Transform the mesh into a cell
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->generate_edge_set();


    #if DYNAMIC_MODEL_INDEX == 0
    //Add somee momentum to the nodes 0 and 1
    c->node_lst_[0].set_momentum(vec3(0, 0, 1));
    c->node_lst_[1].set_momentum(vec3(0, 0, 1));
    #endif

    const unsigned nb_edge_before = c->get_edge_set().size();
    const unsigned nb_node_before = c->get_node_lst().size();
    const unsigned nb_face_before = c->get_face_lst().size();

    //Get the edge AB
    auto edge_it = c->get_edge_set().find(edge(0, 1));

    if(edge_it == c->get_edge_set().end()){
        std::cout << "edge ab not found" << std::endl;
        return 1;
    }
    
    edge edge_ab = const_cast<edge&>(*edge_it);


    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);


    edge_set dummy_set;

    //Remove the face ABD ADC and BCD
    lmr.merge_edge(edge_ab, c, dummy_set);

    //Get the ids of the inserted edge
    unsigned new_node_id = c->get_node_lst().size() - 1;

    //Count the number of edges in which the new node is involved
    unsigned nb_edge_with_new_node = std::count_if(
        c->edge_set_.begin(),
        c->edge_set_.end(),
        [new_node_id](const edge& e){
            return e.n1() == new_node_id || e.n2() == new_node_id;
        }
    );


    //Check that the new edges have been added to the edge to check
    bool t1 = dummy_set.size() == nb_edge_with_new_node;

    //Make sure the number of edges in the cell has increased by 3
    bool t2 = c->get_edge_set().size() == nb_edge_before - 3;

    unsigned nb_node_after = std::count_if(c->get_node_lst().begin(), c->get_node_lst().end(), [](const node& n){return n.is_used();});
    unsigned nb_face_after = std::count_if(c->get_face_lst().begin(), c->get_face_lst().end(), [](const face& f){return f.is_used();});

    bool t3 = nb_node_after == nb_node_before - 1;
    bool t4 = nb_face_after == nb_face_before - 2;
 
    //Make sure all the edges of the cell are connected to 2 faces
    const auto& edge_set = c->get_edge_set();

    bool t5 = std::all_of(edge_set.begin(), edge_set.end(), [](const edge& e) -> bool{return e.is_manifold();});

    #if DYNAMIC_MODEL_INDEX == 0

    //Make sure no momentum has been lost
    const vec3 total_momentum = std::accumulate(
        c->get_node_lst().begin(), 
        c->get_node_lst().end(), 
        vec3(0, 0, 0), 
        [](const vec3& a, const node& b){return a + b.momentum();}
    );
    
        bool t6 = total_momentum == vec3(0, 0, 2);
    #else 
        bool t6 = true;
    #endif



    //Make sure that the 2 merged nodes are not part of any edge
    bool t7 = std::all_of(
        c->edge_set_.begin(),
        c->edge_set_.end(),
        [&edge_ab](const edge& e){
            return e.n1() != edge_ab.n1() && e.n2() != edge_ab.n1() && e.n1() != edge_ab.n2() && e.n2() != edge_ab.n2();
        }
    );

    //Make sure that the 2 merged faces are not part of any edge
    bool t8 = std::all_of(
        c->edge_set_.begin(),
        c->edge_set_.end(),
        [&edge_ab](const edge& e){
            return e.f1() != edge_ab.f1() && e.f2() != edge_ab.f1() && e.f1() != edge_ab.f2() && e.f2() != edge_ab.f2();
        }
    );

    //Make sure that no edge in the cell has the same nodes
    bool t9 = std::all_of(
        c->edge_set_.begin(),
        c->edge_set_.end(),
        [](const edge& e){
            return e.n1() != e.n2();
        }
    );



    mesh_writer::write_cell_data_file(std::string("./test_9.vtk"), {c});



    //std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;
    std::cout << "t3 " << t3 << std::endl;
    std::cout << "t4 " << t4 << std::endl;
    std::cout << "t5 " << t5 << std::endl;
    std::cout << "t6 " << t6 << std::endl;
    std::cout << "t7 " << t7 << std::endl;
    std::cout << "t8 " << t8 << std::endl;
    std::cout << "t9 " << t9 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9);
}
//---------------------------------------------------------------------------------------------------------





//---------------------------------------------------------------------------------------------------------
int local_mesh_refiner_tester::get_triangle_score_test(){
    


    std::vector<double> cell_node_pos_lst{
        1,0,0,  0,1,0,  0,0,1, 
        0,0,0,  1,0,0,  0.5, 0.1,0, 
    };



    std::vector<unsigned> cell_face_connectivity{0, 1, 2, 3, 4, 5};
    cell_ptr c = std::make_shared<cell>(cell_node_pos_lst, cell_face_connectivity, 0);
    c->generate_edge_set();
    c->update_all_face_normals_and_areas();


    face& f1 = const_cast<face&>(c->get_face_lst()[0]);
    face& f2 = const_cast<face&>(c->get_face_lst()[1]);

    //Create the local mesh refiner
    local_mesh_refiner lmr(0.1, 0.3);

    //Get the score of the triangle
    auto [score, longest_edge] = lmr.get_triangle_score(c, f1);

    bool t1 = almost_equal(score, 1.);

    auto [score_2, longest_edge_2] = lmr.get_triangle_score(c, f2);
    bool t2 = longest_edge_2.n1() == 3 && longest_edge_2.n2() == 4;

    std::cout << score_2 << std::endl;

    std::cout << "t1 " << t1 << std::endl;
    std::cout << "t2 " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------





//---------------------------------------------------------------------------------------------------------
// Refining a cell must never give two adjacent faces opposite windings. The fixture is a daughter cell
// exactly as cell_divider::create_daughter_cells() handed it to refine_mesh() (coordinates written with 17
// significant digits), taken from iteration 11000 of the paper-config growth run. split_edge() used to
// pick the winding of the 4 faces it creates from the stored normals of the 2 faces it replaces; on this
// cell one of them is stale and points the other way (a merge earlier in the same call folded it over),
// so 3 directed edges ended up shared by two faces. 25 of 2998 daughter cells of that snapshot did this.
int local_mesh_refiner_tester::refine_mesh_keeps_winding_consistent_test(){

    mesh_reader reader(std::string(PROJECT_SOURCE_DIR) + "/test/test_triangulation_modules/test_local_mesh_refiner/split_edge_winding_daughter.vtk");
    std::vector<mesh> cell_mesh_lst = reader.read();
    assert(cell_mesh_lst.size() == 1);

    cell_ptr c = std::make_shared<cell>(cell_mesh_lst[0], 0);
    c->initialize_cell_properties();

    const double l_min = 1e-6;
    local_mesh_refiner lmr(l_min, 3. * l_min, true);
    lmr.refine_mesh(c);

    //On a consistently oriented closed surface every directed edge belongs to exactly one face
    std::map<std::pair<unsigned, unsigned>, unsigned> directed_edge_count;
    for(const face& f : c->get_face_lst()){
        if(!f.is_used()) continue;
        const auto [n1_id, n2_id, n3_id] = f.get_node_ids();
        directed_edge_count[{n1_id, n2_id}]++;
        directed_edge_count[{n2_id, n3_id}]++;
        directed_edge_count[{n3_id, n1_id}]++;
    }
    const auto nb_bad_edges = std::count_if(directed_edge_count.begin(), directed_edge_count.end(),
        [](const auto& edge_count){return edge_count.second != 1;});

    if(nb_bad_edges != 0){
        std::cout << "FAIL: " << nb_bad_edges << " directed edges are shared by two faces after refinement" << std::endl;
        return 1;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
// The winding self-check at the end of refine_mesh(): a face whose winding is flipped on purpose must be
// detected (its 3 edges are then traversed in the same direction by both of their faces), and a refine_mesh()
// call that splits edges must hand back a consistently and outward oriented surface
int local_mesh_refiner_tester::winding_self_check_test(){

    //A closed octahedron with outward normals, V = 4/3 r^3
    const double r = 1e-5;
    mesh m;
    m.node_pos_lst = { r,0,0,  -r,0,0,  0,r,0,  0,-r,0,  0,0,r,  0,0,-r };
    m.face_point_ids = {{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
    cell_ptr c = std::make_shared<cell>(m, 0);
    c->initialize_cell_properties();

    if(local_mesh_refiner::count_inconsistently_wound_edges(c) != 0 || local_mesh_refiner::winding_checksum(c) != 0){
        std::cout << "FAIL: the octahedron is reported as inconsistently wound before any flip" << std::endl;
        return 1;
    }

    //Flip the winding of one face, the first one so that it is also the seed of the re-orientation
    const auto [f0_n1_id, f0_n2_id, f0_n3_id] = c->face_lst_[0].get_node_ids();
    face flipped_face(f0_n3_id, f0_n2_id, f0_n1_id, 0);
    flipped_face.set_is_used(true);
    flipped_face.set_face_type_id(c->face_lst_[0].get_local_face_type_id());
    c->face_lst_[0] = flipped_face;

    const unsigned nb_bad_edges = local_mesh_refiner::count_inconsistently_wound_edges(c);
    if(nb_bad_edges != 3 || local_mesh_refiner::winding_checksum(c) == 0){
        std::cout << "FAIL: a flipped face must make its 3 edges inconsistent, found " << nb_bad_edges
                  << " (checksum " << local_mesh_refiner::winding_checksum(c) << ")" << std::endl;
        return 1;
    }

    //The edges (1.41e-5 m) are longer than l_max, so refine_mesh() splits them (which propagates the wrong
    //winding to the new faces) and then runs the self-check. Splitting flat faces at the middle of their
    //edges leaves the volume unchanged
    local_mesh_refiner lmr(1e-7, 1.2e-5, false);
    lmr.refine_mesh(c);

    double signed_volume = 0.;
    for(const face& f : c->get_face_lst()){
        if(!f.is_used()) continue;
        const auto [n1_id, n2_id, n3_id] = f.get_node_ids();
        signed_volume += c->get_node_lst()[n1_id].pos().dot(c->get_node_lst()[n2_id].pos().cross(c->get_node_lst()[n3_id].pos())) / 6.;
    }
    const double expected_volume = 4. / 3. * r * r * r;

    if(local_mesh_refiner::count_inconsistently_wound_edges(c) != 0 || std::abs(signed_volume - expected_volume) > 1e-9 * expected_volume){
        std::cout << "FAIL: after refine_mesh() " << local_mesh_refiner::count_inconsistently_wound_edges(c)
                  << " edges are inconsistently wound and the signed volume is " << signed_volume
                  << " m^3 (expected " << expected_volume << " m^3)" << std::endl;
        return 1;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helpers of the refine_meshes() tests
//---------------------------------------------------------------------------------------------------------
namespace {

//The fixture mesh of refine_mesh_keeps_winding_consistent_test(): a daughter cell of the paper-config run, with
//edges of the scale of l_min = 1e-6 m. refine_mesh() has real work on it (64 edges shorter than l_min and 12 sliver
//triangles). Do not use test_1.vtk instead: it is a 10-node mesh at the scale of metres, refine_mesh() does nothing to it.
mesh read_refiner_fixture(){
    mesh_reader reader(std::string(PROJECT_SOURCE_DIR) + "/test/test_triangulation_modules/test_local_mesh_refiner/split_edge_winding_daughter.vtk");
    std::vector<mesh> cell_mesh_lst = reader.read();
    if(cell_mesh_lst.size() != 1) throw std::runtime_error("the refiner fixture must hold exactly one cell");
    return cell_mesh_lst[0];
}

constexpr double fixture_l_min = 1e-6;

//The cell number i of a test population: the fixture translated along x by i * 2.5e-6 m, so that no two cells of a
//population are bit-identical (a refine_meshes() that mixes the cells up, or skips one, is then visible). A cell
//type is not given, so no random number is drawn: building the same cell twice gives the same cell
cell_ptr build_cell(const mesh& fixture, const unsigned i){
    mesh m = fixture;
    for(size_t k = 0; k < m.node_pos_lst.size(); k += 3) m.node_pos_lst[k] += i * 2.5e-6;
    cell_ptr c = std::make_shared<cell>(m, i);
    c->initialize_cell_properties();
    return c;
}

std::vector<cell_ptr> build_population(const mesh& fixture, const unsigned nb_cells){
    std::vector<cell_ptr> cells;
    for(unsigned i = 0; i < nb_cells; i++) cells.push_back(build_cell(fixture, i));
    return cells;
}

bool same_bits(const double x, const double y){return std::memcmp(&x, &y, sizeof(double)) == 0;}

//Describe the first difference between 2 cells, or return "" if there is none. Identical means: the same is_used()
//flags of the nodes and faces, the same node-id triples in the used faces and bit-identical positions of the used
//nodes. The node and face lists are compared as they are (no rebase() before: it would hide a different order)
std::string first_difference(const cell_ptr& a, const cell_ptr& b){
    const std::vector<node>& nodes_a = a->get_node_lst();
    const std::vector<node>& nodes_b = b->get_node_lst();
    if(nodes_a.size() != nodes_b.size()){
        return "node lists have " + std::to_string(nodes_a.size()) + " and " + std::to_string(nodes_b.size()) + " slots";
    }
    for(size_t i = 0; i < nodes_a.size(); i++){
        if(nodes_a[i].is_used() != nodes_b[i].is_used()) return "node " + std::to_string(i) + " is used in only one of the cells";
        if(!nodes_a[i].is_used()) continue;
        const vec3& pa = nodes_a[i].pos();
        const vec3& pb = nodes_b[i].pos();
        if(!same_bits(pa.dx(), pb.dx()) || !same_bits(pa.dy(), pb.dy()) || !same_bits(pa.dz(), pb.dz())){
            return "node " + std::to_string(i) + " is at a different position";
        }
    }

    const std::vector<face>& faces_a = a->get_face_lst();
    const std::vector<face>& faces_b = b->get_face_lst();
    if(faces_a.size() != faces_b.size()){
        return "face lists have " + std::to_string(faces_a.size()) + " and " + std::to_string(faces_b.size()) + " slots";
    }
    for(size_t i = 0; i < faces_a.size(); i++){
        if(faces_a[i].is_used() != faces_b[i].is_used()) return "face " + std::to_string(i) + " is used in only one of the cells";
        if(!faces_a[i].is_used()) continue;
        if(faces_a[i].get_node_ids() != faces_b[i].get_node_ids()) return "face " + std::to_string(i) + " has different nodes";
    }
    return "";
}

//An error that refine_meshes() must contain, with the what() of the exception it throws
struct injected_error{
    std::string name;
    std::function<void()> raise;
    std::string what;
};

std::vector<injected_error> contained_errors(){
    std::vector<injected_error> errors = {
        {"std::bad_optional_access", [](){throw std::bad_optional_access();}, ""},
        {"mesh_integrity_exception", [](){throw mesh_integrity_exception("The refinement of the mesh of cell 17 failed. The simulation is unstable. Reducing the time step might help");}, ""},
        {"std::out_of_range", [](){throw std::out_of_range("vector::_M_range_check: __n (which is 7) >= this->size() (which is 3)");}, ""},
    };
    for(injected_error& e : errors){
        try{e.raise();}
        catch(const std::exception& ex){e.what = ex.what();}
    }
    return errors;
}

//Name the type of the exception that an exception_ptr holds, among the types the fatal-error test cares about
std::string classify(const std::exception_ptr& p){
    if(!p) return "nothing";
    try{std::rethrow_exception(p);}
    catch(const unstable_simulation_exception&){return "unstable_simulation_exception";}
    catch(const std::bad_alloc&){return "std::bad_alloc";}
    catch(const std::exception&){return "another std::exception";}
    catch(const int v){return v == 42 ? "int 42" : "another int";}
    catch(...){return "something else";}
}

} //namespace
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// refine_meshes() must contain the failure of one cell (AC1). Three cells {h1, broken, h2}: the refinement of
// "broken" throws (injected through the refine_meshes_impl() test seam: std::bad_optional_access, then
// mesh_integrity_exception, then std::out_of_range), the others go through the real refine_mesh(). The call must
// not throw, report exactly the broken cell with the what() of the injected exception, and leave h1 and h2 exactly
// as refine_mesh() alone leaves a twin built from the same mesh
int local_mesh_refiner_tester::refine_meshes_contains_failure_test(){

    const mesh fixture = read_refiner_fixture();
    const local_mesh_refiner lmr(fixture_l_min, 3. * fixture_l_min, true);

    bool ok = true;
    for(const injected_error& err : contained_errors()){

        const auto check = [&](const bool condition, const std::string& description){
            if(!condition){
                std::cout << "FAIL [" << err.name << "]: " << description << std::endl;
                ok = false;
            }
        };

        const std::vector<cell_ptr> cells = build_population(fixture, 3);   //{h1, broken, h2}
        const std::vector<cell_ptr> twins = build_population(fixture, 3);   //the same cells, refined alone
        const cell_ptr broken = cells[1];

        std::vector<local_mesh_refiner::refinement_failure> failures;
        bool escaped = false;
        try{
            failures = lmr.refine_meshes_impl(cells, [&](const cell_ptr& c){
                if(c == broken) err.raise();
                lmr.refine_mesh(c);
            });
        }
        catch(const std::exception& e){
            escaped = true;
            std::cout << "FAIL [" << err.name << "]: " << e.what() << " left refine_meshes_impl()" << std::endl;
            ok = false;
        }
        catch(...){
            escaped = true;
            std::cout << "FAIL [" << err.name << "]: an exception left refine_meshes_impl()" << std::endl;
            ok = false;
        }
        if(escaped) continue;

        check(failures.size() == 1, "expected exactly one failure, got " + std::to_string(failures.size()));
        if(failures.size() == 1){
            check(failures[0].cell == broken, "the reported failure is not the broken cell");
            check(!failures[0].reason.empty() && failures[0].reason == err.what,
                  "the reason is \"" + failures[0].reason + "\" instead of the what() of the injected exception, \"" + err.what + "\"");
        }

        for(const unsigned i : {0u, 2u}){
            lmr.refine_mesh(twins[i]);
            const std::string difference = first_difference(cells[i], twins[i]);
            check(difference.empty(), "healthy cell " + std::to_string(i) + " differs from its twin refined alone: " + difference);
            check(!first_difference(build_cell(fixture, i), cells[i]).empty(),
                  "healthy cell " + std::to_string(i) + " was not changed by the refinement (the fixture must give refine_mesh() work to do)");
        }
    }
    return ok ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// The same with many cells and several threads (AC2). 64 cells, every second one fails (the three error types in
// turn), for 1 and for 4 threads with a static schedule. The failures reported must be exactly the failing cells,
// each once with its own what(), and the 32 healthy cells must equal their twins refined alone.
// A shared failure list that is not protected is lost or corrupted only if 2 threads append at the same moment,
// which is rare when the cells in between take milliseconds to refine. So the k-th failing cell of each thread's
// block waits (best effort, at most 200 ms) until every thread has reached its own k-th failing cell, and they
// all throw together: 8 rounds of simultaneous failures per run with 4 threads. The rendezvous only makes a
// missing critical section show up reliably, a round that times out on a loaded machine is not an error
int local_mesh_refiner_tester::refine_meshes_contains_failure_threads_test(){

    const mesh fixture = read_refiner_fixture();
    const local_mesh_refiner lmr(fixture_l_min, 3. * fixture_l_min, true);
    const std::vector<injected_error> errors = contained_errors();
    constexpr unsigned nb_cells = 64;

    const int saved_nb_threads = omp_get_max_threads();
    bool ok = true;

    for(const int nb_threads : {1, 4}){

        const std::string label = "[" + std::to_string(nb_threads) + " thread(s)] ";
        const auto check = [&](const bool condition, const std::string& description){
            if(!condition){
                std::cout << "FAIL " << label << description << std::endl;
                ok = false;
            }
        };

        omp_set_num_threads(nb_threads);
        if(omp_get_max_threads() != nb_threads){
            std::cout << "FAIL " << label << "could not set the number of threads (omp_get_max_threads() = " << omp_get_max_threads() << ")" << std::endl;
            ok = false;
            continue;
        }
        omp_set_schedule(omp_sched_static, 0);   //consecutive blocks of nb_cells / nb_threads cells, one per thread

        const std::vector<cell_ptr> cells = build_population(fixture, nb_cells);
        const std::vector<cell_ptr> twins = build_population(fixture, nb_cells);

        std::map<const cell*, unsigned> index_of;
        for(unsigned i = 0; i < nb_cells; i++) index_of[cells[i].get()] = i;

        const auto fails = [](const unsigned i){return i % 2 == 0;};
        const auto error_of = [&errors](const unsigned i) -> const injected_error& {return errors[(i / 2) % errors.size()];};

        //Thread t refines the block of cells [t * block, (t + 1) * block). Its failing cells are the even offsets in
        //the block, so the k-th failing cell of every thread is at offset 2 k: the threads meet there
        const unsigned block = nb_cells / nb_threads;
        std::vector<std::atomic<int>> nb_arrived(block / 2);
        for(auto& a : nb_arrived) a.store(0);

        std::vector<local_mesh_refiner::refinement_failure> failures;
        bool escaped = false;
        try{
            failures = lmr.refine_meshes_impl(cells, [&](const cell_ptr& c){
                const unsigned i = index_of.at(c.get());
                if(fails(i)){
                    std::atomic<int>& arrived = nb_arrived[(i % block) / 2];
                    arrived++;
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                    while(arrived.load() < nb_threads && std::chrono::steady_clock::now() < deadline){}
                    error_of(i).raise();
                }
                lmr.refine_mesh(c);
            });
        }
        catch(const std::exception& e){
            escaped = true;
            check(false, std::string("an exception left refine_meshes_impl(): ") + e.what());
        }
        catch(...){
            escaped = true;
            check(false, "an exception left refine_meshes_impl()");
        }
        if(escaped) continue;

        const auto nb_incomplete_rounds = std::count_if(nb_arrived.begin(), nb_arrived.end(), [nb_threads](const std::atomic<int>& a){return a.load() != nb_threads;});
        if(nb_incomplete_rounds > 0){
            std::cout << "note " << label << nb_incomplete_rounds << " of " << nb_arrived.size() << " rendezvous rounds timed out (loaded machine?), the check is less sensitive to a race" << std::endl;
        }

        //Exactly the failing cells, each once
        std::set<const cell*> expected_failed;
        for(unsigned i = 0; i < nb_cells; i++) if(fails(i)) expected_failed.insert(cells[i].get());
        std::set<const cell*> reported_failed;
        for(const auto& f : failures) reported_failed.insert(f.cell.get());
        check(failures.size() == expected_failed.size(), "expected " + std::to_string(expected_failed.size()) + " failures, got " + std::to_string(failures.size()));
        check(reported_failed == expected_failed, "the set of failed cells is not the set of cells that were made to fail");

        //Each with the what() of its own exception
        unsigned nb_wrong_reasons = 0;
        for(const auto& f : failures){
            const auto it = index_of.find(f.cell.get());
            if(it == index_of.end() || f.reason != error_of(it->second).what) nb_wrong_reasons++;
        }
        check(nb_wrong_reasons == 0, std::to_string(nb_wrong_reasons) + " failures carry the reason of another cell or none");

        //The healthy cells are the ones refine_mesh() gives when it runs alone
        unsigned nb_different = 0, nb_unchanged = 0;
        for(unsigned i = 0; i < nb_cells; i++){
            if(fails(i)) continue;
            lmr.refine_mesh(twins[i]);
            if(!first_difference(cells[i], twins[i]).empty()) nb_different++;
            if(first_difference(build_cell(fixture, i), cells[i]).empty()) nb_unchanged++;
        }
        check(nb_different == 0, std::to_string(nb_different) + " of the 32 healthy cells differ from their twin refined alone");
        check(nb_unchanged == 0, std::to_string(nb_unchanged) + " of the 32 healthy cells were not refined at all");
    }

    omp_set_num_threads(saved_nb_threads);
    return ok ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// The errors that must still stop the run leave refine_meshes() with their own type (AC3):
// unstable_simulation_exception (the domain has diverged), std::bad_alloc, and an exception that is not a
// std::exception at all (throw 42). Also when a contained failure happens in the same call, and with 4 threads
int local_mesh_refiner_tester::refine_meshes_rethrows_fatal_test(){

    const mesh fixture = read_refiner_fixture();
    const local_mesh_refiner lmr(fixture_l_min, 3. * fixture_l_min, true);

    //Run refine_meshes_impl() on 4 cells and return what leaves it: the fatal error is thrown for cell 2, and,
    //if with_contained_failure is set, a contained one for cell 1
    const auto escaped_from_impl = [&](const std::function<void()>& raise_fatal, const bool with_contained_failure) -> std::exception_ptr {
        const std::vector<cell_ptr> cells = build_population(fixture, 4);
        const cell_ptr fatal_cell = cells[2];
        const cell_ptr contained_cell = cells[1];
        try{
            (void) lmr.refine_meshes_impl(cells, [&](const cell_ptr& c){
                if(c == fatal_cell) raise_fatal();
                if(with_contained_failure && c == contained_cell) throw std::out_of_range("a failure that is contained");
                lmr.refine_mesh(c);
            });
        }
        catch(...){return std::current_exception();}
        return nullptr;
    };

    struct fatal_case{
        std::string expected;
        std::function<void()> raise;
    };
    const std::vector<fatal_case> cases = {
        {"unstable_simulation_exception", [](){throw unstable_simulation_exception("a node coordinate is not finite");}},
        {"std::bad_alloc",                [](){throw std::bad_alloc();}},
        {"int 42",                        [](){throw 42;}},
    };

    const int saved_nb_threads = omp_get_max_threads();
    bool ok = true;
    for(const int nb_threads : {1, 4}){
        omp_set_num_threads(nb_threads);
        if(omp_get_max_threads() != nb_threads){
            std::cout << "FAIL: could not set the number of threads to " << nb_threads << std::endl;
            ok = false;
            continue;
        }
        omp_set_schedule(omp_sched_static, 0);

        for(const fatal_case& fc : cases){
            for(const bool with_contained_failure : {false, true}){
                const std::string got = classify(escaped_from_impl(fc.raise, with_contained_failure));
                if(got != fc.expected){
                    std::cout << "FAIL [" << nb_threads << " thread(s), " << (with_contained_failure ? "with" : "without") << " a contained failure]: "
                              << "expected " << fc.expected << " to leave refine_meshes_impl(), got " << got << std::endl;
                    ok = false;
                }
            }
        }
    }
    omp_set_num_threads(saved_nb_threads);
    return ok ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// The public wrapper, without any injection (R6). Unlike the tests that use the private test seam, this one also
// runs in Debug and AddressSanitizer builds: refine_meshes() on healthy cells returns no failure, and every cell
// ends exactly as if refine_mesh() had been called on it alone
int local_mesh_refiner_tester::refine_meshes_public_wrapper_test(){

    const mesh fixture = read_refiner_fixture();
    const local_mesh_refiner lmr(fixture_l_min, 3. * fixture_l_min, true);
    constexpr unsigned nb_cells = 8;

    const std::vector<cell_ptr> cells = build_population(fixture, nb_cells);
    const std::vector<cell_ptr> twins = build_population(fixture, nb_cells);

    const std::vector<local_mesh_refiner::refinement_failure> failures = lmr.refine_meshes(cells);

    bool ok = true;
    if(!failures.empty()){
        std::cout << "FAIL: refine_meshes() reported " << failures.size() << " failures on healthy cells, the first one: " << failures[0].reason << std::endl;
        ok = false;
    }

    for(unsigned i = 0; i < nb_cells; i++){
        lmr.refine_mesh(twins[i]);
        const std::string difference = first_difference(cells[i], twins[i]);
        if(!difference.empty()){
            std::cout << "FAIL: cell " << i << " differs from its twin refined alone: " << difference << std::endl;
            ok = false;
        }
        if(first_difference(build_cell(fixture, i), cells[i]).empty()){
            std::cout << "FAIL: cell " << i << " was not changed by refine_meshes() (the fixture must give it work to do)" << std::endl;
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// The main function
int main (int argc, char** argv){

    //Check that the command line input is correctly formatted
    assert(argc == 2); 

    //Get the name of the test to run
    std::string test_name = argv[1];

    local_mesh_refiner_tester tester;
    
    //Run the selected test

    if (test_name == "can_be_merged_test")             return tester.can_be_merged_test();

    //if (test_name == "prepare_merger_test_1")         return tester.prepare_merger_test_1();
    //if (test_name == "prepare_merger_test_2")         return tester.prepare_merger_test_2();
    if (test_name == "edge_swap_test")                return tester.edge_swap_test();
    if (test_name == "split_edge_test")               return tester.split_edge_test();
    if (test_name == "merge_edge_test")               return tester.merge_edge_test();
    if (test_name == "get_triangle_score_test")       return tester.get_triangle_score_test();
    if (test_name == "refine_mesh_keeps_winding_consistent_test") return tester.refine_mesh_keeps_winding_consistent_test();
    if (test_name == "winding_self_check_test")       return tester.winding_self_check_test();
    if (test_name == "refine_meshes_contains_failure_test")         return tester.refine_meshes_contains_failure_test();
    if (test_name == "refine_meshes_contains_failure_threads_test") return tester.refine_meshes_contains_failure_threads_test();
    if (test_name == "refine_meshes_rethrows_fatal_test")           return tester.refine_meshes_rethrows_fatal_test();
    if (test_name == "refine_meshes_public_wrapper_test")           return tester.refine_meshes_public_wrapper_test();

    
    

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------

