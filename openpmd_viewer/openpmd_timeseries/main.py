"""
This file is part of the openPMD-viewer.

It defines the main OpenPMDTimeSeries class.

Copyright 2015-2016, openPMD-viewer contributors
Authors: Remi Lehe, Axel Huebl
License: 3-Clause-BSD-LBNL
"""
import time
import libfbindex
import geosindex
import numpy as np
import pandas as pd
from scipy import constants
from tqdm import tqdm

from .data_reader import DataReader, available_backends
from .interactive import InteractiveViewer
from .particle_tracker import ParticleTracker
from .plotter import Plotter
from .utilities import apply_selection, fit_bins_to_grid, try_array, \
    sanitize_slicing, combine_cylindrical_components

# Define a custom Exception
class OpenPMDException(Exception):
    "Exception raised for invalid use of the openPMD-viewer API"
    pass


class OpenPMDTimeSeries(InteractiveViewer):
    """
    Main class for the exploration of an openPMD timeseries

    For more details, see the docstring of the following methods:
    - get_field
    - get_particle
    - slider
    """

    def __init__(self, path_to_dir, check_all_files=True, backend=None, 
                    geos_index=False,
                    fastbit_index=False,
                    geos_index_type="minmax",
                    geos_index_storage_backend = "file", 
                    geos_index_save_path=None,
                    geos_index_secondary_type = "none",
                    key_generation_function=None):
        """
        Initialize an openPMD time series

        More precisely, scan the directory and extract the openPMD files,
        as well as some useful openPMD parameters

        Parameters
        ----------
        path_to_dir: string
            The path to the directory where the openPMD files are.

        check_all_files: bool, optional
            Check that all the files in the timeseries are consistent
            (i.e. that they contain the same fields and particles,
            with the same metadata)
            For fast access to the files, this can be changed to False.

        backend: string
            Backend to be used for data reading. Can be `openpmd-api`
            or `h5py`. If not provided will use `openpmd-api` if available
            and `h5py` otherwise.
        """
        # Check backend
        if backend is None:
            backend = available_backends[0] #Pick openpmd-api first if available
        elif backend not in available_backends:
            raise RuntimeError("Invalid backend requested: {0}\n"
                    "The available backends are: {1}"
                    .format(backend, available_backends) )
        self.backend = backend
        self.geos_index = geos_index
        self.fastbit_index = fastbit_index
        if self.geos_index:
            self.geos_index_type = geos_index_type
            self.geos_index_storage_backend = geos_index_storage_backend
            self.geos_index_secondary_type = geos_index_secondary_type

            if geos_index_type == "minmax":
                self.query_geos_index = geosindex.MinMaxQuery(geos_index_save_path, geos_index_storage_backend, geos_index_secondary_type)
            elif geos_index_type == "rtree":
                self.query_geos_index = geosindex.RTreeQuery(geos_index_save_path, geos_index_storage_backend, geos_index_secondary_type)

        if key_generation_function:
            self.key_generation_function = key_generation_function
        else:
            self.key_generation_function = lambda iteration, species, type, dimension=None: f"/data/{iteration}/particles/{species}/{type}/" + (f"{dimension}" if dimension else "")

        if self.fastbit_index:
            self.query_fastbit_index = libfbindex.GeosFastBitQuery(geos_index_save_path, geos_index_storage_backend)

        # Initialize data reader
        self.data_reader = DataReader(backend)

        # Extract the iterations available in this timeseries
        self.iterations = self.data_reader.list_iterations(path_to_dir)

        # Check that there are files in this directory
        if len(self.iterations) == 0:
            print("Error: Found no valid files in the specified directory.\n"
                  "Please check that this is the path to the openPMD files.")
            return(None)

        # Go through the files of the series, extract the time
        # and a few parameters.
        N_iterations = len(self.iterations)
        self.t = np.zeros(N_iterations)

        # - Extract parameters from the first file
        t, params0 = self.data_reader.read_openPMD_params(self.iterations[0])
        self.t[0] = t
        self.extensions = params0['extensions']
        self.avail_fields = params0['avail_fields']
        if self.avail_fields is not None:
            self.fields_metadata = params0['fields_metadata']
            self.avail_geom = set( self.fields_metadata[field]['geometry']
                                for field in self.avail_fields )
        # Extract information of the particles
        self.avail_species = params0['avail_species']
        self.avail_record_components = \
            params0['avail_record_components']

        # - Extract the time for each file and, if requested, check
        #   that the other files have the same parameters
        for k in range(1, N_iterations):
            t, params = self.data_reader.read_openPMD_params(
                self.iterations[k], check_all_files)
            self.t[k] = t
            if check_all_files:
                for key in params0.keys():
                    if params != params0:
                        print("Warning: File %s has different openPMD "
                              "parameters than the rest of the time series."
                              % self.iterations[k])
                        break

        # - Set the current iteration and time
        self._current_i = 0
        self.current_iteration = self.iterations[0]
        self.current_t = self.t[0]
        # - Find the min and the max of the time
        self.tmin = self.t.min()
        self.tmax = self.t.max()

        # - Initialize a plotter object, which holds information about the time
        self.plotter = Plotter(self.t, self.iterations)

        # - gc read strategy
        self.max_level = 999
        self.max_read_length = 10000000000
        self.sorted_blocks = None
        self.read_strategy = list()
        if "large" in path_to_dir or "openPMD" in path_to_dir:
            self.k = 1.86*10e-8
            self.b = 6.2*10e-3
        else:
            self.k = 3.35*10e-9
            self.b = 6.2*10e-4


    def result_to_tuple(self, result_obj):
        return result_obj[1].start, result_obj[1].end, None

    def batch_to_tuple(self, result_obj):
        return result_obj[0], result_obj[0] + result_obj[1], None

    def find_optimal_strategy(self, start, end, current_level):
        """
        Finds the optimal strategy for reading a list of one-dimensional points.

        Args:
            start (int): The start index of the list.
            end (int): The end index of the list, inclusive.
            current_level (int): The current level of the recursion.

        Returns:
            A tuple containing the optimal cost and the list of indices to cut the list at.
        """
        if current_level > self.max_level or start == end:
            self.read_strategy.append((start, end))
            return 

        # calculate the maximum gap between points
        max_gap = 0
        left_end = 0
        right_start = 0
        for i in range(start, end):
            gap = int(self.sorted_blocks[i+1][1].start) - int(self.sorted_blocks[i][1].end)
            if gap > max_gap:
                max_gap = gap
                left_end = i
                right_start = i+1

        if max_gap == 0:
            self.read_strategy.append((start, end))
            return

        # original_cost = (int(self.sorted_blocks[-1]) - int(self.sorted_blocks[0])) * k + b
        # new_cost = (int(self.sorted_blocks[left_end]) - int(self.sorted_blocks[0])) * k + b + (int(self.sorted_blocks[-1] - int(self.sorted_blocks[right_start]) * k + b
        # new_cost - original_cost = (int(self.sorted_blocks[left_end] - int(self.sorted_blocks[right_start]) * k + b
        # temp1 = (int(self.sorted_blocks[left_end][1].start) - int(self.sorted_blocks[right_start][1].end)) * self.k + self.b
        # temp2 = int(self.sorted_blocks[left_end][1].end) - int(self.sorted_blocks[start][1].start)
        # temp3 = int(self.sorted_blocks[end][1].end) - int(self.sorted_blocks[right_start][1].start)
        if (int(self.sorted_blocks[left_end][1].start) - int(self.sorted_blocks[right_start][1].end)) * self.k + self.b < 0 \
                or int(self.sorted_blocks[left_end][1].end) - int(self.sorted_blocks[start][1].start) > self.max_read_length \
                or int(self.sorted_blocks[end][1].end) - int(self.sorted_blocks[right_start][1].start) > self.max_read_length:
            # if the cost of reading 2 times is less than the cost of reading once, then read twice
            self.find_optimal_strategy(start, left_end, current_level+1)
            self.find_optimal_strategy(right_start, end, current_level+1)
        else:
            self.read_strategy.append((start, end))
            return 

    def get_particle(self, var_list=None, species=None, t=None, iteration=None,
            select=None, plot=False, nbins=150,
            plot_range=[[None, None], [None, None]],
            use_field_mesh=True, histogram_deposition='cic', 
            geos_index_use_secondary = False,
            geos_index_direct_block_read = True,
            geos_index_read_groups = False,
            skip_offset=False,
            limit_block_num=None,
            limit_memory_usage=None,
            block_meta_path=None,
            memory_usage_factor=1.0,
            **kw):
        """
        Extract a list of particle variables an openPMD file.

        Plot the histogram of the returned quantity.
        If two quantities are requested by the user, this plots
        a 2d histogram of these quantities.

        In the case of momenta, the result is returned as:
        - unitless momentum (i.e. gamma*beta) for particles with non-zero mass
        - in kg.m.s^-1 for particles with zero mass

        Parameters
        ----------
        var_list : list of string, optional
            A list of the particle variables to extract. If var_list is not
            provided, the available particle quantities are printed

        species: string
            A string indicating the name of the species
            This is optional if there is only one species

        t : float (in seconds), optional
            Time at which to obtain the data (if this does not correspond to
            an existing iteration, the closest existing iteration will be used)
            Either `t` or `iteration` should be given by the user.

        iteration : int
            The iteration at which to obtain the data
            Either `t` or `iteration` should be given by the user.

        select: dict or ParticleTracker object, optional
            - If `select` is a dictionary:
            then it lists a set of rules to select the particles, of the form
            'x' : [-4., 10.]   (Particles having x between -4 and 10 meters)
            'ux' : [-0.1, 0.1] (Particles having ux between -0.1 and 0.1 mc)
            'uz' : [5., None]  (Particles with uz above 5 mc)
            - If `select` is a ParticleTracker object:
            then it returns particles that have been selected at another
            iteration ; see the docstring of `ParticleTracker` for more info.

        plot : bool, optional
           Whether to plot the requested quantity
           Plotting support is only available when requesting one or two
           quantities (i.e. when var_list is of length 1 or 2)

        nbins : int, optional
           (Only used when `plot` is True)
           Number of bins for the histograms

        plot_range : list of lists
           A list containing 2 lists of 2 elements each
           Indicates the values between which to perform the histogram,
           along the 1st axis (first list) and 2nd axis (second list)
           Default: the range is automatically determined

        use_field_mesh: bool, optional
           (Only used when `plot` is True)
           Whether to use the information of the spatial mesh (whenever
           possible) in order to choose the parameters of the histograms.
           More precisely, when this is True:
           - The extent of the histogram (along any spatial dimension) is
             automatically chosen to be roughly the extent of the spatial mesh.
           - The number of bins (along any spatial dimension) is slightly
             modified (from the value `nbins` provided by the user) so that
             the spacing of the histogram is an integer multiple of the grid
             spacing. This avoids artifacts in the plot, whenever particles
             are regularly spaced in each cell of the spatial mesh.

        histogram_deposition : string
            Either `ngp` (Nearest Grid Point) or `cic` (Cloud-In-Cell)
            When plotting the particle histogram, this determines how
            particles affects neighboring bins.
            `cic` (which is the default) leads to smoother results than `ngp`.

        **kw : dict, otional
           Additional options to be passed to matplotlib's
           hist or hist2d.

        Returns
        -------
        A list of 1darray corresponding to the data requested in `var_list`
        (one 1darray per element of 'var_list', returned in the same order)
        """
        # Check that the species required are present
        if self.avail_species is None:
            raise OpenPMDException('No particle data in this time series')
        # If there is only one species, infer that the user asks for that one
        if species is None and len(self.avail_species) == 1:
            species = self.avail_species[0]
        if species not in self.avail_species:
            species_list = '\n - '.join(self.avail_species)
            raise OpenPMDException(
                "The argument `species` is missing or erroneous.\n"
                "The available species are: \n - %s\nPlease set the "
                "argument `species` accordingly." % species_list)

        # Check the list of variables
        valid_var_list = True
        if not isinstance(var_list, list):
            valid_var_list = False
        else:
            for quantity in var_list:
                if quantity not in self.avail_record_components[species]:
                    valid_var_list = False
        if not valid_var_list:
            quantity_list = '\n - '.join(
                self.avail_record_components[species])
            raise OpenPMDException(
                "The argument `var_list` is missing or erroneous.\n"
                "It should be a list of strings representing species record "
                "components.\n The available quantities for species '%s' are:"
                "\n - %s\nPlease set the argument `var_list` "
                "accordingly." % (species, quantity_list) )

        # Check the format of the particle selection
        if select is None or isinstance(select, ParticleTracker):
            pass
        elif isinstance(select, dict):
            # Dictionary: Check that all selection quantities are available
            valid_select_list = True
            for quantity in select.keys():
                if not (quantity in self.avail_record_components[species]):
                    valid_select_list = False
            if not valid_select_list:
                quantity_list = '\n - '.join(
                    self.avail_record_components[species])
                raise OpenPMDException(
                    "The argument `select` is erroneous.\n"
                    "It should be a dictionary whose keys represent particle "
                    "quantities.\n The available quantities are: "
                    "\n - %s\nPlease set the argument `select` "
                    "accordingly." % quantity_list)
        else:
            raise OpenPMDException("The argument `select` is erroneous.\n"
            "It should be either a dictionary or a ParticleTracker object.")

        # Find the output that corresponds to the requested time/iteration
        # (Modifies self._current_i, self.current_iteration and self.current_t)
        self._find_output(t, iteration)
        # Get the corresponding iteration
        iteration = self.iterations[self._current_i]

        # Extract the list of particle quantities
        data_list = []
        if not self.geos_index or not select:
            # fastbit method
            if self.fastbit_index:
                dict_record_comp = {'x': ['position', 'x'],
                    'y': ['position', 'y'],
                    'z': ['position', 'z'],
                    'ux': ['momentum', 'x'],
                    'uy': ['momentum', 'y'],
                    'uz': ['momentum', 'z'],
                    'w': ['weighting', None]}
                
                if species == "electrons":
                    mass = 9.1093829099999999e-31
                elif species == "hydrogen":
                    mass = 1.6726219236900000e-27

                momentum_constant = 1. / (mass * constants.c)

                # read block meta info
                block_meta_df = pd.read_csv(block_meta_path, sep=',', header=None, names=['iteration', 'block_start', 'block_count'])
                block_meta_df = block_meta_df[block_meta_df['iteration'] == iteration]
                block_meta_df = block_meta_df.sort_values(by=['block_start'])
                # remove duplicate
                block_meta_df = block_meta_df.drop_duplicates(subset=['block_start'])
                block_meta_df = block_meta_df.reset_index(drop=True)

                data_map = dict()
                for block_id, row in block_meta_df.iterrows():
                    start = time.time()
                    block_result = list()
                    for quantity in select.keys():
                        fb_key = self.key_generation_function(iteration=iteration, species=species, type=dict_record_comp[quantity][0], dimension=dict_record_comp[quantity][1])
                        
                        lower_bound = select[quantity][0] 
                        upper_bound = select[quantity][1] 

                        if quantity in {'ux', 'uy', 'uz'}:
                            lower_bound /= momentum_constant
                            upper_bound /= momentum_constant

                        # print(fb_key, block_id, row['block_count'], lower_bound, upper_bound)
                        # print(fb_key)
                        # print(block_id)
                        # print(row['block_count'])
                        # print(select[quantity][0]/momentum_constant)
                        # print(select[quantity][1]/momentum_constant)
                        # print(select[quantity][0])
                        # print(select[quantity][1])
                        # if dict_record_comp[quantity][0] == "momentum":
                        #     result = self.query_fastbit_index.queryFastbitData(fb_key, 0, block_id, row['block_count'], select[quantity][0]/momentum_constant, select[quantity][1]/momentum_constant)
                        # else:
                        #     result = self.query_fastbit_index.queryFastbitData(fb_key, 0, block_id, row['block_count'], select[quantity][0], select[quantity][1])
                        result = self.query_fastbit_index.queryFastbitData(fb_key, 0, block_id, row['block_count'], lower_bound, upper_bound)
                        block_result.append(result)
                    
                    # intersect the result
                    block_idx_set = set(block_result[0])
                    if len(block_result) > 1:
                        for i in range(1, len(block_result)):
                            block_idx_set = block_idx_set.intersection(set(block_result[i]))
                    end = time.time()
                    print("Fastbit Index cost. Time elapsed: ", end - start)

                    if len(block_idx_set) == 0:
                        continue
                    print(len(block_idx_set))
                    # print(block_idx_set)

                    self.read_chunk_range = list()
                    self.read_chunk_range.append((row['block_start'], row['block_start'] + row['block_count'], None))

                    start = time.time()
                    for quantity in var_list:
                        if quantity not in data_map.keys():
                            data_map[quantity] = list()

                        data = self.data_reader.read_species_data(
                            iteration, species, quantity, self.extensions, self.read_chunk_range, False)
                        
                        # only keep the data that is in the intersection
                        data = data[list(block_idx_set)]
                        data_map[quantity].append(data)
    
                        del data
                    end = time.time()
                    print("Fastbit Read Data Cost. Time elapsed: ", end - start)

                start = time.time()
                for quantity in data_map.keys():
                    data_list.append(np.concatenate(data_map[quantity]))
                end = time.time()
                print("Fastbit Concatenate Data. Time elapsed: ", end - start)

            elif limit_memory_usage is not None:
                # limit_memory_usage = 64GB
                # Determine the number of particles
                max_N = int(int(limit_memory_usage.replace("GB", "")) * 1024**3 / 8 / memory_usage_factor)
                # read block meta info
                block_meta_df = pd.read_csv(block_meta_path, sep=',', header=None, names=['iteration', 'block_start', 'block_count'])
                block_meta_df = block_meta_df[block_meta_df['iteration'] == iteration]
                block_meta_df = block_meta_df.sort_values(by=['block_start'])
                # remove duplicate
                block_meta_df = block_meta_df.drop_duplicates(subset=['block_start'])
                block_meta_df = block_meta_df.reset_index(drop=True)

                read_batch = [[]]
                current_n = 0
                current_batch = 0
                for index, row in block_meta_df.iterrows():
                    if current_n + row['block_count'] > max_N:
                        current_batch += 1
                        read_batch.append([])
                        current_n = 0

                    current_n += row['block_count']
                    read_batch[current_batch].append((row['block_start'], row['block_count']))

                select_array_list = list()
                for batch in read_batch:
                    self.read_chunk_range = list(map(self.batch_to_tuple, batch))
                    batch_total_particle = sum([x[1] for x in batch])

                    select_array = np.ones(batch_total_particle, dtype='bool')
                    # Loop through the selection rules, and aggregate results in select_array
                    for quantity in select.keys():
                        q = self.data_reader.read_species_data(
                            iteration, species, quantity, self.extensions, self.read_chunk_range, skip_offset)
                        
                        start = time.time()
                        # Check lower bound
                        if select[quantity][0] is not None:
                            select_array = np.logical_and(
                                select_array,
                                q > select[quantity][0])
                        # Check upper bound
                        if select[quantity][1] is not None:
                            select_array = np.logical_and(
                                select_array,
                                q < select[quantity][1])
                        end = time.time()
                        print("calculate particle level select array. Time elapsed: ", end - start)

                        del q

                    select_array_list.append(select_array)
                
                data_map = dict()
                i = 0
                for batch in read_batch:
                    self.read_chunk_range = list(map(self.batch_to_tuple, batch))

                    for quantity in var_list:
                        if quantity not in data_map.keys():
                            data_map[quantity] = list()

                        data = self.data_reader.read_species_data(
                            iteration, species, quantity, self.extensions, self.read_chunk_range, skip_offset)
                
                        start = time.time()
                        data = data[select_array_list[i]]
                        end = time.time()
                        print("apply particle level select array. Time elapsed: ", end - start)
                        data_map[quantity].append(data)
                        
                        del data

                    i += 1

                for quantity in data_map.keys():
                    data_list.append(np.concatenate(data_map[quantity]))

            else:
                # 1. default method
                for quantity in var_list:
                    data_list.append( self.data_reader.read_species_data(
                        iteration, species, quantity, self.extensions))

                # Apply selection if needed
                if isinstance( select, dict ):
                    data_list = apply_selection( iteration, self.data_reader,
                        data_list, select, species, self.extensions)
                elif isinstance( select, ParticleTracker ):
                    data_list = select.extract_tracked_particles( iteration,
                        self.data_reader, data_list, species, self.extensions )
            if len(data_list) != 0:
                print(len(data_list[0]))

        # Use the geos_index to select particles
        else:
            # 1. [multiple columns] use geos_index to coarse selection return [a list of [key/block, start, count]]
            if isinstance( select, dict ):
                dict_record_comp = {'x': ['position', 'x'],
                    'y': ['position', 'y'],
                    'z': ['position', 'z'],
                    'ux': ['momentum', 'x'],
                    'uy': ['momentum', 'y'],
                    'uz': ['momentum', 'z'],
                    'w': ['weighting', None]}

                if species == "electrons":
                    mass = 9.1093829099999999e-31
                elif species == "hydrogen":
                    mass = 1.6726219236900000e-27

                momentum_constant = 1. / (mass * constants.c)

                # query_result is list(std::map<std::string, QueryBlockResult>), block_start as the key
                query_result = list()
                data_map = dict()
                data_size = None

                start = time.time()
                if self.geos_index_type=="minmax":
                    for quantity in select.keys():
                        key = self.key_generation_function(iteration=iteration, species=species, type=dict_record_comp[quantity][0], dimension=dict_record_comp[quantity][1])
                        
                        if dict_record_comp[quantity][0] == "position" or np.isinf(select[quantity][0]) or np.isinf(select[quantity][1]):
                            result = self.query_geos_index.queryMinMaxData(key, select[quantity][0], select[quantity][1])
                        elif dict_record_comp[quantity][0] == "momentum":
                            result = self.query_geos_index.queryMinMaxData(key, select[quantity][0]/momentum_constant, select[quantity][1]/momentum_constant)

                        # query_result includes max_6 members: dicts of position_xyz and momentum_xyz, then direct take interaction
                        query_result.append(result)

                elif self.geos_index_type == "rtree":
                    select_map = dict()
                    for quantity in select.keys():
                        if dict_record_comp[quantity][0] not in select_map.keys():
                            # initialize a new dict for the quantity, i.e. position or momentum, 3d envolope
                            select_map[dict_record_comp[quantity][0]] = dict()
                            select_map[dict_record_comp[quantity][0]]["minx"] = -np.inf
                            select_map[dict_record_comp[quantity][0]]["maxx"] = np.inf
                            select_map[dict_record_comp[quantity][0]]["miny"] = -np.inf
                            select_map[dict_record_comp[quantity][0]]["maxy"] = np.inf
                            select_map[dict_record_comp[quantity][0]]["minz"] = -np.inf
                            select_map[dict_record_comp[quantity][0]]["maxz"] = np.inf

                        if np.isinf(select[quantity][0]) and np.isinf(select[quantity][1]):
                            continue
                        
                        select_map[dict_record_comp[quantity][0]]["min" + dict_record_comp[quantity][1]] = select[quantity][0]
                        select_map[dict_record_comp[quantity][0]]["max" + dict_record_comp[quantity][1]] = select[quantity][1]

                        if dict_record_comp[quantity][0] == "momentum":
                            select_map[dict_record_comp[quantity][0]]["min" + dict_record_comp[quantity][1]] /= momentum_constant
                            select_map[dict_record_comp[quantity][0]]["max" + dict_record_comp[quantity][1]] /= momentum_constant

                    for i, select_type in enumerate(select_map.keys()):
                        key = self.key_generation_function(iteration=iteration, species=species, type=select_type)
                        result = self.query_geos_index.queryRTreeXYZ(key, 
                                                                        select_map[select_type]["minx"],
                                                                        select_map[select_type]["maxx"],
                                                                        select_map[select_type]["miny"],
                                                                        select_map[select_type]["maxy"],
                                                                        select_map[select_type]["minz"],
                                                                        select_map[select_type]["maxz"])
                        query_result.append(result)

                end = time.time()
                print("query index: Time elapsed: ", end - start)

                # intersect the result, use the first one as the base
                if len(query_result) > 1:
                    start = time.time()
                    block_idx_set = set(query_result[0].keys())
                    for i in range(1, len(query_result)):
                        # interact the result
                        block_idx_set = block_idx_set.intersection(set(query_result[i].keys()))
                    # remove the block that is not in the intersection
                    for block_start in list(query_result[0].keys()):
                        if block_start not in block_idx_set:
                            del query_result[0][block_start]

                    # end = time.time()
                    # print("intersect the first level index result. Time elapsed: ", end - start)
                    # start = time.time()

                    # Current block is in the other query result, but the secondary slice is not, remove it
                    if self.geos_index_secondary_type != "none" and geos_index_use_secondary:
                        for block_start in list(query_result[0].keys()):
                            slice_start_set = set(query_result[0][block_start].q.keys())
                            for i in range(1, len(query_result)):
                                slice_start_set = slice_start_set.intersection(set(query_result[i][block_start].q.keys()))

                            for slice_start in list(query_result[0][block_start].q.keys()):
                                if slice_start not in slice_start_set:
                                    del query_result[0][block_start].q[slice_start]
                    end = time.time()
                    print("remove duplication. Time elapsed: ", end - start)

                if len(query_result[0]) == 0:
                    return list(), list()

                print("The size of the query result: ", len(query_result[0]))
                if limit_block_num and len(query_result[0]) > limit_block_num:
                    return f"The number of blocks is {len(query_result[0])}, please reduce the range of the selection"

                # read data based on query_result[0]
                self.read_chunk_range = list()
                select_array_secondary = list()
                select_range_secondary = list()
                # [fastest] group nearby blocks and read together
                if geos_index_read_groups:
                    self.read_strategy = list()

                    start = time.time()
                    self.sorted_blocks = sorted(query_result[0].items(), key=lambda x: int(x[0]))
                    end = time.time()
                    print("sort block metadata by block start. Time elapsed: ", end - start)

                    start = time.time()
                    self.find_optimal_strategy(0, len(self.sorted_blocks) - 1, 0)
                    end = time.time()
                    print("find optimal read solution. Time elapsed: ", end - start)

                    # offset = 0
                    for block_start_index, block_end_index in self.read_strategy:
                        # print(block_start_index, block_end_index)
                        self.read_chunk_range.append((self.sorted_blocks[block_start_index][1].start, self.sorted_blocks[block_end_index][1].end, None))
                        # for i in range(block_start_index, block_end_index + 1):
                        #     select_range.append((offset, self.sorted_blocks[i][1].end - self.sorted_blocks[i][1].start + offset))
                        #     offset += self.sorted_blocks[i][1].end - self.sorted_blocks[i][1].start
                    

                    # generate the mask for the data
                    start = time.time()
                    select_array_length = sum([x[1] - x[0] for x in self.read_chunk_range])
                    select_array_secondary = np.zeros(select_array_length, dtype='bool')
                    for block_start_index, block_end_index in self.read_strategy:
                        for range_index in range(block_start_index, block_end_index + 1):
                            if self.geos_index_secondary_type != "none" and geos_index_use_secondary:
                                # for each slice,
                                for slice_key, slice_obj in self.sorted_blocks[range_index][1].q.items():
                                    select_array_secondary[slice_obj.start - self.sorted_blocks[block_start_index][1].start: slice_obj.end - self.sorted_blocks[block_start_index][1].start] = True
                            else:
                                # include the start
                                select_array_secondary[self.sorted_blocks[range_index][1].start - self.sorted_blocks[block_start_index][1].start:
                                                self.sorted_blocks[range_index][1].end - self.sorted_blocks[block_start_index][1].start] = True

                    # for block_start_index, block_end_index in self.read_strategy:
                    #     select_array_secondary.append(np.zeros(self.sorted_blocks[block_end_index][1].end - self.sorted_blocks[block_start_index][1].start, dtype='bool'))
                    #     for range_index in range(block_start_index, block_end_index + 1):
                    #         if self.geos_index_secondary_type != "none" and geos_index_use_secondary:
                    #             # for each slice,
                    #             for slice_key, slice_obj in self.sorted_blocks[range_index][1].q.items():
                    #                 select_array_secondary[-1][slice_obj.start - self.sorted_blocks[block_start_index][1].start: slice_obj.end - self.sorted_blocks[block_start_index][1].start] = True
                    #         else:
                    #             # include the start and end
                    #             select_array_secondary[-1][self.sorted_blocks[range_index][1].start - self.sorted_blocks[block_start_index][1].start:
                    #                             self.sorted_blocks[range_index][1].end - self.sorted_blocks[block_start_index][1].start] = True
                    select_array_secondary = np.concatenate(select_array_secondary)
                    end = time.time()
                    print("generate select array. Time elapsed: ", end - start)
                    

                # [middle] direct read block
                elif geos_index_direct_block_read:
                    start = time.time()
                    self.read_chunk_range = list(map(self.result_to_tuple, query_result[0].items()))

                    # generate the mask for the data, if use secondary slice
                    if self.geos_index_secondary_type != "none" and geos_index_use_secondary:
                        self.sorted_blocks = sorted(query_result[0].items(), key=lambda x: int(x[0]))
                        for block_start in list(query_result[0].keys()):
                            select_array_secondary.append(np.zeros(query_result[0][block_start].end - query_result[0][block_start].start, dtype='bool'))
                            for slice_key, slice_obj in query_result[0][block_start].q.items():
                                select_array_secondary[-1][slice_obj.start - query_result[0][block_start].start: slice_obj.end - query_result[0][block_start].start] = True
                        select_array_secondary = np.concatenate(select_array_secondary)

                    end = time.time()
                    print("Direct block read. generate select array. Time elapsed: ", end - start)

                # [slowest] direct read secondary slice
                # not geos_index_read_groups and not geos_index_direct_block_read
                elif self.geos_index_secondary_type != "none" and geos_index_use_secondary:
                    # which means to read by the secondary slice
                    # no mask
                    start = time.time()
                    for block_start in list(query_result[0].keys()):
                        temp = query_result[0][block_start].q.items()
                        self.read_chunk_range += list(map(self.result_to_tuple, temp))
                    end = time.time()
                    print("Direct slice read. generate select array. Time elapsed: ", end - start)


                else:
                    print("Error: No valid geos_index read strategy")
                    return list(), list()

                # read data based on the read_chunk_range
                for quantity in set(var_list + list(select.keys())):
                    print("size of self.read_chunk_range: ", len(self.read_chunk_range))
                    data_map[quantity] = self.data_reader.read_species_data(iteration, species, quantity, self.extensions, self.read_chunk_range, skip_offset)
                    # if len(select_range) > 0 and not select_all_flag:
                    #     start = time.time()
                    #     data_map[quantity] = data_map[quantity][select_array]
                    #     # data_map[quantity] = np.hstack([data_map[quantity][range_local[0]:range_local[1]] for range_local in select_range])
                    #     end = time.time()
                    #     print("apply particle level select array. Time elapsed: ", end - start)
                    data_size = len(data_map[quantity])
                    print(quantity, data_size)

                start = time.time()
                if len(select_array_secondary) > 0:
                    for quantity in data_map.keys():
                        if len(data_map[quantity]) > 1:
                            data_map[quantity] = data_map[quantity][select_array_secondary]
                            data_size = len(data_map[quantity])
                end = time.time()
                print("apply secondary slice select array. Time elapsed: ", end - start)

                # Linear match the remaining data
                data_list = list()
  
                select_array_particle = np.ones(data_size, dtype='bool')
                for quantity in select.keys():
                    if skip_offset and quantity in {'ux', 'uy', 'uz'}:
                        select[quantity][0] /= momentum_constant
                        select[quantity][1] /= momentum_constant

                    start = time.time()
                    # Check lower bound
                    if select[quantity][0] is not None:
                        print("The lower bound of", quantity, "is", select[quantity][0])
                        select_array_particle = np.logical_and(
                            select_array_particle,
                            data_map[quantity] > select[quantity][0])
                    # Check upper bound
                    if select[quantity][1] is not None:
                        print("The upper bound of", quantity, "is", select[quantity][1])
                        select_array_particle = np.logical_and(
                            select_array_particle,
                            data_map[quantity] < select[quantity][1])
                    end = time.time()
                    print("calculate particle level select array. Time elapsed: ", end - start)

                start = time.time()
                # Use select_array_particle to reduce each quantity
                for key in var_list:
                    if len(data_map[key]) > 1:  # Do not apply selection on scalar records
                        data_map[key] = data_map[key][select_array_particle]
                end = time.time()
                print("apply particle level select array. Time elapsed: ", end - start)

                if skip_offset:
                    for quantity in select.keys():
                        # read support data
                        # start = time.time()
                        support_quantity_data = self.data_reader.read_species_support_data(iteration, species, quantity, self.extensions, self.read_chunk_range, skip_offset)
                        # end = time.time()
                        # print(f"get support data for read {quantity}. Time elapsed: ", end - start)
                        
                        start_time = time.time()
                        # if len(select_range) > 0:
                        #     # support_quantity_data = support_quantity_data[select_array]
                        #     # support_quantity_data = np.hstack([support_quantity_data[range_local[0]:range_local[1]] for range_local in select_range])
                        #     total_length = sum(end - start for start, end in select_range)
                        #     new_array_prealloc = np.empty(total_length, dtype=support_quantity_data.dtype)
                        #     current_position = 0
                        #     for start, end in select_range:
                        #         length = end - start
                        #         new_array_prealloc[current_position:current_position + length] = support_quantity_data[start:end]
                        #         current_position += length

                        #     del support_quantity_data
                        #     support_quantity_data = new_array_prealloc

                        if len(select_array_secondary) > 0:
                            select_quantity_data = support_quantity_data[select_array_secondary]

                        support_quantity_data = support_quantity_data[select_array_particle]

                        if quantity in {'ux', 'uy', 'uz'} and quantity in var_list:
                            if np.all( support_quantity_data != 0 ):
                                support_quantity_data *= constants.c
                                temp = np.full_like(support_quantity_data, 1.0)
                                temp /= support_quantity_data
                                data_map[quantity] *= temp
                        elif quantity in {'x', 'y', 'z'} and quantity in var_list:
                            data_map[quantity] += support_quantity_data
                        end_time = time.time()
                        print("data apply support data. Time elapsed: ", end_time - start_time)

                        del support_quantity_data

                for key in var_list:
                    if len(data_map[key]) > 1:  # Do not apply selection on scalar records
                        data_list.append(data_map[key])
                print("selected data length: ", len(data_list[0]))

                              
            elif isinstance( select, ParticleTracker ):
                data_list = select.extract_tracked_particles( iteration,
                    self.data_reader, var_list, species, self.extensions )
                # todo particle tracing
                # pass
        # print()
        # Plotting
        if plot and len(var_list) in [1, 2]:

            # Extract the weights, if they are available
            if 'w' in self.avail_record_components[species]:
                w = self.data_reader.read_species_data(
                    iteration, species, 'w', self.extensions)
                if isinstance( select, dict ):
                    w, = apply_selection( iteration, self.data_reader,
                        [w], select, species, self.extensions)
                elif isinstance( select, ParticleTracker ):
                    w, = select.extract_tracked_particles( iteration,
                        self.data_reader, [w], species, self.extensions )
            # Otherwise consider that all particles have a weight of 1
            else:
                w = np.ones_like(data_list[0])

            # Determine the size of the histogram bins
            # - First pick default values
            hist_range = [[None, None], [None, None]]
            for i_data in range(len(data_list)):
                data = data_list[i_data]

                # Check if the user specified a value
                if (plot_range[i_data][0] is not None) and \
                        (plot_range[i_data][1] is not None):
                    hist_range[i_data] = plot_range[i_data]
                # Else use min and max of data
                elif len(data) != 0:
                    hist_range[i_data] = [ data.min(), data.max() ]
                else:
                    hist_range[i_data] = [ -1., 1. ]

                # Avoid error when the min and max are equal
                if hist_range[i_data][0] == hist_range[i_data][1]:
                    if hist_range[i_data][0] == 0:
                        hist_range[i_data] = [ -1., 1. ]
                    else:
                        hist_range[i_data][0] *= 0.99
                        hist_range[i_data][1] *= 1.01

            hist_bins = [ nbins for i_data in range(len(data_list)) ]
            # - Then, if required by the user, modify this values by
            #   fitting them to the spatial grid
            if use_field_mesh and self.avail_fields is not None:
                # Extract the grid resolution
                grid_size_dict, grid_range_dict = \
                    self.data_reader.get_grid_parameters( iteration,
                        self.avail_fields, self.fields_metadata )
                # For each direction, modify the number of bins, so that
                # the resolution is a multiple of the grid resolution
                for i_var in range(len(var_list)):
                    var = var_list[i_var]
                    if var in grid_size_dict.keys():
                        # Check that the user indeed allowed this dimension
                        # to be determined automatically
                        if (plot_range[i_var][0] is None) or \
                                (plot_range[i_var][1] is None):
                            hist_bins[i_var], hist_range[i_var] = \
                                fit_bins_to_grid(hist_bins[i_var],
                                grid_size_dict[var], grid_range_dict[var] )

            # - In the case of only one quantity
            if len(data_list) == 1:
                # Do the plotting
                self.plotter.hist1d(data_list[0], w, var_list[0], species,
                        self._current_i, hist_bins[0], hist_range,
                        deposition=histogram_deposition, **kw)
            # - In the case of two quantities
            elif len(data_list) == 2:
                # Do the plotting
                self.plotter.hist2d(data_list[0], data_list[1], w,
                    var_list[0], var_list[1], species,
                    self._current_i, hist_bins, hist_range,
                    deposition=histogram_deposition, **kw)

        # Output the data
        return(data_list)

    def get_field(self, field=None, coord=None, t=None, iteration=None,
                  m='all', theta=0., slice_across=None,
                  slice_relative_position=None, plot=False,
                  plot_range=[[None, None], [None, None]], **kw):
        """
        Extract a given field from a file in the openPMD format.

        Parameters
        ----------

        field : string, optional
           Which field to extract

        coord : string, optional
           Which component of the field to extract

        m : int or str, optional
           Only used for thetaMode geometry
           Either 'all' (for the sum of all the modes)
           or an integer (for the selection of a particular mode)

        t : float (in seconds), optional
            Time at which to obtain the data (if this does not correspond to
            an existing iteration, the closest existing iteration will be used)
            Either `t` or `iteration` should be given by the user.

        iteration : int
            The iteration at which to obtain the data
            Either `t` or `iteration` should be given by the user.

        theta : float or None, optional
           Only used for thetaMode geometry
           The angle of the plane of observation, with respect to the x axis
           If `theta` is not None, then this function returns a 2D array
           corresponding to the plane of observation given by `theta` ;
           otherwise it returns a full 3D Cartesian array

        slice_across : str or list of str, optional
           Direction(s) across which the data should be sliced
           + In cartesian geometry, elements can be:
               - 1d: 'z'
               - 2d: 'x' and/or 'z'
               - 3d: 'x' and/or 'y' and/or 'z'
           + In cylindrical geometry, elements can be 'r' and/or 'z'
           Returned array is reduced by 1 dimension per slicing.
           If slicing is None, the full grid is returned.

        slice_relative_position : float or list of float, optional
           Number(s) between -1 and 1 that indicate where to slice the data,
           along the directions in `slice_across`
           -1 : lower edge of the simulation box
           0 : middle of the simulation box
           1 : upper edge of the simulation box
           Default: None, which results in slicing at 0 in all direction
           of `slice_across`.

        plot : bool, optional
           Whether to plot the requested quantity

        plot_range : list of lists
           A list containing 2 lists of 2 elements each
           Indicates the values between which to clip the plot,
           along the 1st axis (first list) and 2nd axis (second list)
           Default: plots the full extent of the simulation box

        **kw : dict, otional
           Additional options to be passed to matplotlib's imshow.

        Returns
        -------
        A tuple with
           F : a 2darray containing the required field
           info : a FieldMetaInformation object
           (see the corresponding docstring)
        """
        # Check that the field required is present
        if self.avail_fields is None:
            raise OpenPMDException('No field data in this time series')
        # Check the field type
        if field not in self.avail_fields:
            field_list = '\n - '.join(self.avail_fields)
            raise OpenPMDException(
                "The `field` argument is missing or erroneous.\n"
                "The available fields are: \n - %s\nPlease set the `field` "
                "argument accordingly." % field_list)
        # Check slicing
        slice_across, slice_relative_position = \
            sanitize_slicing(slice_across, slice_relative_position)
        if slice_across is not None:
            # Check that the elements are valid
            axis_labels = self.fields_metadata[field]['axis_labels']
            for axis in slice_across:
                if axis not in axis_labels:
                    axes_list = '\n - '.join(axis_labels)
                    raise OpenPMDException(
                    'The `slice_across` argument is erroneous: contains %s\n'
                    'The available axes are: \n - %s' % (axis, axes_list) )

        # Check the coordinate, for vector fields
        if self.fields_metadata[field]['type'] == 'vector':
            available_coord = ['x', 'y', 'z']
            if self.fields_metadata[field]['geometry'] == 'thetaMode':
                available_coord += ['r', 't']
            if coord not in available_coord:
                coord_list = '\n - '.join(available_coord)
                raise OpenPMDException(
                    "The field %s is a vector field, \nbut the `coord` "
                    "argument is missing or erroneous.\nThe available "
                    "coordinates are: \n - %s\nPlease set the `coord` "
                    "argument accordingly." % (field, coord_list))
        # Automatically set the coordinate to None, for scalar fields
        else:
            coord = None

        # Check the mode (for thetaMode)
        if self.fields_metadata[field]['geometry'] == "thetaMode":
            avail_circ_modes = self.fields_metadata[field]['avail_circ_modes']
            if str(m) not in avail_circ_modes:
                mode_list = '\n - '.join(avail_circ_modes)
                raise OpenPMDException(
                    "The requested mode '%s' is not available.\n"
                    "The available modes are: \n - %s" % (m, mode_list))

        # Find the output that corresponds to the requested time/iteration
        # (Modifies self._current_i, self.current_iteration and self.current_t)
        self._find_output(t, iteration)
        # Get the corresponding iteration
        iteration = self.iterations[self._current_i]

        # Find the proper path for vector or scalar fields
        if self.fields_metadata[field]['type'] == 'scalar':
            field_label = field
        elif self.fields_metadata[field]['type'] == 'vector':
            field_label = field + coord

        # Get the field data
        geometry = self.fields_metadata[field]['geometry']
        axis_labels = self.fields_metadata[field]['axis_labels']
        # - For cartesian
        if geometry in ["1dcartesian", "2dcartesian", "3dcartesian"]:
            F, info = self.data_reader.read_field_cartesian(
                iteration, field, coord, axis_labels,
                slice_relative_position, slice_across)
        # - For thetaMode
        elif geometry == "thetaMode":
            if (coord in ['x', 'y']) and \
                    (self.fields_metadata[field]['type'] == 'vector'):
                # For Cartesian components, combine r and t components
                Fr, info = self.data_reader.read_field_circ(
                    iteration, field, 'r', slice_relative_position,
                    slice_across, m, theta)
                Ft, info = self.data_reader.read_field_circ(
                    iteration, field, 't', slice_relative_position,
                    slice_across, m, theta)
                F = combine_cylindrical_components(Fr, Ft, theta, coord, info)
            else:
                # For cylindrical or scalar components, no special treatment
                F, info = self.data_reader.read_field_circ(iteration,
                    field, coord, slice_relative_position,
                    slice_across, m, theta)

        # Plot the resulting field
        # Deactivate plotting when there is no slice selection
        if plot:
            if F.ndim == 1:
                self.plotter.show_field_1d(F, info, field_label,
                self._current_i, plot_range=plot_range, **kw)
            elif F.ndim == 2:
                self.plotter.show_field_2d(F, info, slice_across, m,
                    field_label, geometry, self._current_i,
                    plot_range=plot_range, **kw)
            else:
                raise OpenPMDException('Cannot plot %d-dimensional data.\n'
                    'Use the argument `slice_across`, or set `plot=False`' % F.ndim)

        # Return the result
        return(F, info)

    def iterate( self, called_method, *args, **kwargs ):
        """
        Repeated calls the method `called_method` for every iteration of this
        timeseries, with the arguments `*args` and `*kwargs`.

        The result of these calls is returned as a list, or, whenever possible
        as an array, where the first axis corresponds to the iterations.

        If `called_method` returns a tuple/list, then `iterate` returns a
        tuple/list of lists (or arrays).

        Parameters
        ----------
        *args, **kwargs: arguments and keyword arguments
            Arguments that would normally be passed to `called_method` for
            a single iteration. Do not pass the argument `t` or `iteration`.
        """
        # Add the iteration key in the keyword aguments
        kwargs['iteration'] = self.iterations[0]

        # Check the shape of results
        result = called_method(*args, **kwargs)
        result_type = type( result )
        if result_type in [tuple, list]:
            returns_iterable = True
            iterable_length = len(result)
            accumulated_result = [ [element] for element in result ]
        else:
            returns_iterable = False
            accumulated_result = [ result ]

        # Call the method for all iterations
        for iteration in tqdm(self.iterations[1:]):
            kwargs['iteration'] = iteration
            result = called_method( *args, **kwargs )
            if returns_iterable:
                for i in range(iterable_length):
                    accumulated_result[i].append( result[i] )
            else:
                accumulated_result.append( result )

        # Try to stack the arrays
        if returns_iterable:
            for i in range(iterable_length):
                accumulated_result[i] = try_array( accumulated_result[i] )
            if result_type == tuple:
                return tuple(accumulated_result)
            elif result_type == list:
                return accumulated_result
        else:
            accumulated_result = try_array( accumulated_result )
            return accumulated_result

    def _find_output(self, t, iteration):
        """
        Find the output that correspond to the requested `t` or `iteration`
        Modify self._current_i accordingly.

        Parameter
        ---------
        t : float (in seconds)
            Time requested

        iteration : int
            Iteration requested
        """
        # Check the arguments
        if (t is not None) and (iteration is not None):
            raise OpenPMDException(
                "Please pass either a time (`t`) \nor an "
                "iteration (`iteration`), but not both.")
        # If a time is requested
        elif (t is not None):
            # Make sure the time requested does not exceed the allowed bounds
            if t < self.tmin:
                self._current_i = 0
            elif t > self.tmax:
                self._current_i = len(self.t) - 1
            # Find the closest existing iteration
            else:
                self._current_i = abs(self.t - t).argmin()
        # If an iteration is requested
        elif (iteration is not None):
            if (iteration in self.iterations):
                # Get the index that corresponds to this iteration
                self._current_i = abs(iteration - self.iterations).argmin()
            else:
                iter_list = '\n - '.join([str(it) for it in self.iterations])
                raise OpenPMDException(
                      "The requested iteration '%s' is not available.\nThe "
                      "available iterations are: \n - %s\n" % (iteration, iter_list))
        else:
            pass  # self._current_i retains its previous value

        # Register the value in the object
        self.current_t = self.t[self._current_i]
        self.current_iteration = self.iterations[self._current_i]
