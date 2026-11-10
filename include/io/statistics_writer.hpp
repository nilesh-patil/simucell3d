#ifndef DEF_STATISTICS_WRITER
#define DEF_STATISTICS_WRITER

#include <string>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <sstream>
#include <cassert>
#include <vector>
#include <chrono>

#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "utils.hpp"
#include "mesh_data.hpp"
#include "cell.hpp"



/**
 * @file
 * @brief Writers of the per-cell statistics, to a CSV file or to a string.
 *
 * The columns are `iteration`, `computation_time_(hh::mm:ss)`, `simulation_time`, then one column per
 * mapper in file_data_mapper_lst (mesh_data.hpp); adding a mapper there adds a column. Each row ends with
 * the separator.
 * @ingroup io
 */




//-----------------------------------------------------------------------------------------------
/**
 * @brief Interface of the statistics writers. The solver calls write_data() every 50 iterations and once at
 * the end of the run.
 * @ingroup io
 */
class abstract_statistics_writer{
    protected:
        std::string sep = ",";

        std::chrono::time_point<std::chrono::system_clock> starting_time_point_;

    public:
        abstract_statistics_writer() = default;                                              //default constructor
        virtual ~abstract_statistics_writer() = default;                                     //virtual destructor for polymorphic deletion
        abstract_statistics_writer(const abstract_statistics_writer& v) = delete;            //copy constructor
        abstract_statistics_writer(abstract_statistics_writer&& v) = delete;                 //move constructor
        abstract_statistics_writer& operator=(const abstract_statistics_writer& v) = delete; //copy assignment operator
        abstract_statistics_writer& operator=(abstract_statistics_writer&& v) = default;     //move assignment operator

        /**
         * @brief Append one row per cell.
         * @param iteration Current iteration.
         * @param simulation_time Simulated time, s.
         * @param cell_lst Cells to record.
         */
        virtual void write_data(
            const unsigned iteration, 
            const double simulation_time,  
            const std::vector<cell_ptr>& cell_lst
        ) noexcept(false) = 0;
};
//-----------------------------------------------------------------------------------------------




//-----------------------------------------------------------------------------------------------
/**
 * @brief Writes the statistics to a CSV file (`simulation_statistics.csv` in the output folder).
 * @ingroup io
 */
class csv_file_statistics_writer: public abstract_statistics_writer{
    private:

        //The path to the csv file where the data will be written
        std::string output_file_path_;

    public:
        csv_file_statistics_writer() = default;                                    //default constructor
        csv_file_statistics_writer(const csv_file_statistics_writer& v) = delete;           //copy constructor
        csv_file_statistics_writer(csv_file_statistics_writer&& v) = delete;                //move constructor
        csv_file_statistics_writer& operator=(const csv_file_statistics_writer& v) = delete;//copy assignment operator
        csv_file_statistics_writer& operator=(csv_file_statistics_writer&& v) = default;     //move assignment operator 

        /**
         * @brief Create (or overwrite) the file and write the header row.
         * @throws intialization_exception If the file cannot be created.
         */
        csv_file_statistics_writer(const std::string& output_file_path) noexcept(false);


        /**
         * @brief Append one row per cell to the file.
         * @throws intialization_exception If the file cannot be opened or written.
         */
        void write_data(
            const unsigned iteration, 
            const double simulation_time,  
            const std::vector<cell_ptr>& cell_lst
        ) noexcept(false) override;


};
//-----------------------------------------------------------------------------------------------








//-----------------------------------------------------------------------------------------------
/**
 * @brief Collects the statistics in memory; used when the simulation runs through the Python module.
 * @ingroup io
 */
class string_statistics_writer: public abstract_statistics_writer{
    private:

        //The data will be written in this string
        std::stringstream str_stream;


    public:                              //default constructor
        string_statistics_writer(const string_statistics_writer& v) = delete;           //copy constructor
        string_statistics_writer(string_statistics_writer&& v) = delete;                //move constructor
        string_statistics_writer& operator=(const string_statistics_writer& v) = delete;//copy assignment operator
        string_statistics_writer& operator=(string_statistics_writer&& v) = default;     //move assignment operator 

        /// Start the string with the header row.
        string_statistics_writer() noexcept(false);

        /// Append one row per cell to the string.
        void write_data(
            const unsigned iteration, 
            const double simulation_time,  
            const std::vector<cell_ptr>& cell_lst
        ) noexcept(false) override;

        /// Return the statistics collected so far, in CSV format.
        std::string get_string() const noexcept(false){return str_stream.str();}
};
//-----------------------------------------------------------------------------------------------


#endif