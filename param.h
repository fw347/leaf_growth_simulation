/*
 Basic diffusion and FRAP simulation
 Jonathan Ward and Francois Nedelec, Copyright EMBL 2007-2009
 */

#include <cmath>
#include <sstream>
const double SCALING_FACTOR = 100000;

// physical parameters:  ensure to add any new parameters to the readOption() function
double xBound = 50 * SCALING_FACTOR;   /// half-width of box (X) in micrometers. If this value is lower than 100 the deulaunay triangulation misses points
double yBound = xBound;   /// half-height of box (Y), is set to be equal to y for saftey
double pixel = 1;    /// size of one pixel in GL units

int nbo = 5;    /// number of particles
double delta = 0.00001;   /// currently useless
const double fluidViscosity = 0.0016; /// Pa.s, velocity of water at 20 degrees celcius
const double mobilityCoefficient = 6 * 3.14159 * fluidViscosity;
const double breakSpringCoeff = 5;

double timestep = 0.00006; /// viscosity is in Pa.sec so this is seconds. 60 fps means 1sec simulated = 1.8sec realtime
int delay = 16;         /// milli-seconds between successive display
unsigned long seed = 2; /// seed for random number generator

double hormone1ProdRate = 10;
double hormone1DegRate = 0.008;
double hormone1IntroTime = 0.005;
vector2D hormone1Origin = vector2D(0.25*xBound,0.15*xBound);
double hormone1DiffCoeff = 0.03 * SCALING_FACTOR;
double hormone1DiffPro = 0.06; /// this is made up i think,
double hormEfficacy = 0.3;

double maxProbOfDiv = 0.005; /// should inform this with the maximal amount of cell division that can occur, dont think this should be tunable
double desiredTotalCells = 100; /// used to calculate mitosis probabilites


double realTime = 0;     /// time in the simulated world



//-----------------------------------------------------------------------------

template <typename T>
int readParameter(const char arg[], const char name[], T & ptr)
{
    if ( arg == strstr(arg, name) ) {
        std::istringstream iss(arg+strlen(name));
        iss >> ptr;
        return !iss.fail();
    }
    return 0;
}


int readOption(const char arg[])
{
    if ( readParameter(arg, "n=",     nbo) )    return 1;
    if ( readParameter(arg, "delta=", delta) )  return 1;
    if ( readParameter(arg, "seed=",  seed) )   return 1;
    if ( readParameter(arg, "delay=", delay) )  return 1;
    if ( readParameter(arg, "bounds=", xBound) )  return 1;


    return 0;
}

