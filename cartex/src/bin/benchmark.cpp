/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <thread>

#include <cartex/common/options.hpp>
#include <cartex/runtime/runtime.hpp>

int
main(int argc, char** argv)
{
    /* clang-format off */
    const auto options = cartex::options()
        ("domain",   "local domain size",              "X Y Z",    {64,64,64})
        ("nrep",     "number of repetitions",          "r",        {10})
        ("nfields",  "number of fields",               "n",        {1})
        ("halo",     "halo size",                      "h",        {1})
        ("MPICart",  "MPI cartesian global grid",      "NX NY NZ", 3)
        ("order",    "cartesian order (default: XYZ)", "IJK",      1)
        ("node",     "node grid",                      "NX NY NZ", 3)
        ("socket",   "socket grid",                    "NX NY NZ", 3)
        ("numa",     "numa-node grid",                 "NX NY NZ", 3)
        ("l3",       "l3-cache grid",                  "NX NY NZ", 3)
        ("core",     "core grid",                      "NX NY NZ", 3)
        ("hwthread", "hardware-thread grid",           "NX NY NZ", 3)
        ("thread",   "software-thread grid",           "NX NY NZ", {1,1,1})
        ("print",    "print decomposition")
        ("check",    "check results")
        .parse(argc, argv);
    /* clang-format on */

    const auto threads = options.get<std::array<int, 3>>("thread");
    const auto num_threads = threads[0] * threads[1] * threads[2];

    int required = num_threads > 1 ? MPI_THREAD_MULTIPLE : MPI_THREAD_SINGLE;
    int provided;
    int init_result = MPI_Init_thread(&argc, &argv, required, &provided);
    if (init_result == MPI_ERR_OTHER)
    {
        std::cerr << "MPI init failed\n";
        std::terminate();
    }
    if (provided < required)
    {
        std::cerr << "MPI does not support required threading level\n";
        std::terminate();
    }

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    {
        std::unique_ptr<cartex::decomposition> decomp_ptr;

        try
        {
            if (options.has("MPICart"))
            {
                if (options.has("order"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --order" << std::endl;
                if (options.has("node"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --node" << std::endl;
                if (options.has("socket"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --socket" << std::endl;
                if (options.has("numa"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --numa" << std::endl;
                if (options.has("l3"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --l3" << std::endl;
                if (options.has("core"))
                    if (rank == 0) std::cerr << "Warning: ignoring argument --core" << std::endl;
                if (options.has("hwthread"))
                    if (rank == 0)
                        std::cerr << "Warning: ignoring argument --hwthread" << std::endl;
                const auto dims = options.get<std::array<int, 3>>("MPICart");
                int        cart_size = dims[0] * dims[1] * dims[2];
                if (cart_size != size)
                    throw std::runtime_error("cart size is not equal to world size");
                decomp_ptr = std::make_unique<cartex::decomposition>(dims, threads);
            }
            /* clang-format off */
        else if (options.has("hwthread"))
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                options.get_or("socket", std::array<int,3>{1,1,1}),
                options.get_or("numa",   std::array<int,3>{1,1,1}),
                options.get_or("l3",     std::array<int,3>{1,1,1}),
                options.get_or("core",   std::array<int,3>{1,1,1}),
                options.get<std::array<int,3>>("hwthread"),
                threads);
        else if (options.has("core"))
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                options.get_or("socket", std::array<int,3>{1,1,1}),
                options.get_or("numa",   std::array<int,3>{1,1,1}),
                options.get_or("l3",     std::array<int,3>{1,1,1}),
                options.get<std::array<int,3>>("core"),
                threads);
        else if (options.has("l3"))
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                options.get_or("socket", std::array<int,3>{1,1,1}),
                options.get_or("numa",   std::array<int,3>{1,1,1}),
                options.get<std::array<int,3>>("l3"),
                threads);
        else if (options.has("numa"))
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                options.get_or("socket", std::array<int,3>{1,1,1}),
                options.get<std::array<int,3>>("numa"),
                threads);
        else if (options.has("socket"))
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                options.get<std::array<int,3>>("socket"),
                threads);
        else 
            decomp_ptr = std::make_unique<cartex::decomposition>(
                options.get_or("order",  std::string("XYZ")),
                options.get_or("node",   std::array<int,3>{1,1,1}),
                threads);
        }
        /* clang-format on */
        catch (...)
        {
            if (rank == 0) std::cout << "could not create decomposition" << std::endl;
            MPI_Finalize();
            return 0;
        }

        if (options.is_set("print"))
        {
            CARTEX_CHECK_MPI_RESULT(MPI_Barrier(decomp_ptr->mpi_comm()));
            decomp_ptr->print();
            CARTEX_CHECK_MPI_RESULT(MPI_Barrier(decomp_ptr->mpi_comm()));
            decomp_ptr.release();
            CARTEX_CHECK_MPI_RESULT(MPI_Barrier(MPI_COMM_WORLD));
            MPI_Finalize();
            return 0;
        }

        cartex::runtime r(options.get<int>("nrep"), options.get<std::array<int, 3>>("domain"),
            options.get<int>("halo"), options.get<int>("nfields"), options.is_set("check"),
            *decomp_ptr);

        if (rank == 0)
        {
            std::cout << r.info() << std::endl;
            std::cout << "running on " << size << " ranks" << std::endl;
        }

        if (decomp_ptr->threads_per_rank() == 1) r.exchange(0);
        else
        {
            std::vector<std::thread> threads;
            threads.reserve(num_threads);
            for (int j = 0; j < num_threads; ++j)
                threads.push_back(std::thread([&r, j]() { r.exchange(j); }));
            for (auto& t : threads) t.join();
        }

        CARTEX_CHECK_MPI_RESULT(MPI_Barrier(decomp_ptr->mpi_comm()));
    }
    MPI_Finalize();
    return 0;
}