"""
This is the public API to ANUGA_PARALLEL.

Ideally, all tools needed to run parallel simulations should be
imported from this module
"""

from parallel_api import distribute
from parallel_api import myid, numprocs, get_processor_name
from parallel_api import send, receive
from parallel_api import pypar_available, barrier, finalize

if pypar_available:
    from parallel_meshes import parallel_rectangle
    from parallel_shallow_water import Parallel_domain as Parallel_shallow_water_domain
    from parallel_advection     import Parallel_domain as Parallel_advection_domain
else:
    from anuga import rectangular_cross as parallel_rectangle
    from anuga import Domain as Parallel_shallow_water_domain
    from anuga.advection import Advection_Domain as Parallel_advection_domain


	
