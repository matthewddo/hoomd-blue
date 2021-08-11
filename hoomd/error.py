# Copyright (c) 2009-2021 The Regents of the University of Michigan
# This file is part of the HOOMD-blue project, released under the BSD 3-Clause
# License.

"""HOOMD Errors."""


class DataAccessError(RuntimeError):
    """Raised when data is inaccessible until the simulation is run."""

    def __init__(self, data_name):
        self.data_name = data_name

    def __str__(self):
        """Returns the error message."""
        return (f'The property {self.data_name} is unavailable until the '
                'simulation runs for 0 or more steps.')


class TypeConversionError(ValueError):
    """Error when validatimg TypeConverter subclasses fails."""
    pass


class SimulationDefinitionError(RuntimeError):
    """Error in definition of simulation internal state."""
    pass
