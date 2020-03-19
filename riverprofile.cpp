/*******************
 *
 *
 *  GRATE 8
 *
 *  Long profile parameters
 *
 *
 *
*********************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include "riverprofile.h"
#include "tinyxml2/tinyxml2.h"
#include "tinyxml2_wrapper.h"

using namespace std;
using namespace tinyxml2;

#define PI 3.14159265
#define G 9.80665
#define RHO 1000  // water density
#define Gs 1.65   // submerged specific gravity

double gammln2(double xx)
{
  double x,y,tmp,ser;
  static double cof[6]={76.18009172947146,    -86.50532032941677,
            24.01409824083091,    -1.231739572450155,
            0.1208650973866179e-2,-0.5395239384953e-5};
  int j;

  y=x=xx;
  tmp=x+5.5;
  tmp -= (x+0.5)*log(tmp);
  ser=1.000000000190015;
  for (j=0;j<=5;j++) ser += cof[j]/++y;
  return -tmp+log(2.5066282746310005*ser/x);
}

NodeGSDObject::NodeGSDObject()
    {
         vector <double> tmp;                  // dummy array for grain sizes
         abrasion.push_back(0.0000060000);
         abrasion.push_back(0.0000060000);
         abrasion.push_back(0.0000060000);

         density.push_back(0.0000060000);
         density.push_back(0.0000060000);
         density.push_back(0.0000060000);

         for (int j = 0; j < 15; j++)
         {
             tmp.push_back(0);
             psi.push_back(-3 + j );            // psi -3 to 11 .. should be 9, but ngsz+1,2 is required throughout
         }

         for (int k = 0; k < 3; k++)
             pct.push_back(tmp);

         dsg = 0;
         stdv = 0;
         sand_pct = 0;
    }

void NodeGSDObject::norm_frac()
// Normalize grainsize fractions to 100%
{
    unsigned int ngsz, nlith;
    double cumtot;

    ngsz = psi.size() - 2;
    nlith = abrasion.size();

    std::vector<double> ktot(ngsz);

    sand_pct = 0;

    // Normalize
    cumtot = 0.0;
    for ( unsigned int j = 0; j < ngsz; j++ )           // Sum mass fractions
    {
        ktot[j] = 0.0;
        for ( unsigned int k = 0; k < nlith; k++ )
            if (pct[k][j] > 0)              // Solves problems with rounding
                ktot[j] += pct[k][j];
            else
                pct[k][j] = 0;
        cumtot += ktot[j];
    }

    for ( unsigned int j = 0; j < ngsz; j++ )
        for ( unsigned int k = 0; k < nlith; k++ )
        {
            if (pct[k][j] > 0)
                pct[k][j] /= cumtot;
            if (psi[j] <= 0) sand_pct += pct[k][j];   // sum sand fraction
        }
}

void NodeGSDObject::dg_and_std()
// Routines to calculate grainsize statistics, including D50,D84,D90 and standard deviation
{
    double tdev;
    unsigned int ngsz, nlith;

    ngsz = psi.size() - 2;
    nlith = abrasion.size();
    
    std::vector<double> ktot(ngsz);

    dsg = 0.0;
    d84 = 0.0;
    d90 = 0.0;

    for ( unsigned int j = 0; j < ngsz; j++ )
    {
        //if (psi[j] >= -1)   // ***!!!*** Dg50 is based on gsizes >= 2 mm (psi = -1 or less)
        //{
        ktot[j] = 0;
        for ( unsigned int k = 0; k < nlith; k++ )
            ktot[j] += pct[k][j];               // lithology values for each size fraction are summed.
        dsg += 0.50 * (psi[j] + psi[j+1]) * ktot[j];
        d84 += 0.84 * (psi[j] + psi[j+1]) * ktot[j];
        d90 += 0.90 * (psi[j] + psi[j+1]) * ktot[j];
        //}
    }

    stdv = 0.0;
    for ( unsigned int j = 0; j < ngsz; j++ )
    {
        tdev = 0.5 * (psi[j] + psi[j+1]) - dsg;
        stdv += 0.5 * tdev * tdev * ktot[j];
    }

    if (stdv > 0)
        stdv = sqrt(stdv);
}

NodeXSObject::NodeXSObject()                  //Initialize list
{
     node = 0;
     noChannels = 1;
     depth = 0.;
     width = 0.;
     wsl = 0.0;
     velocity = 0.;
     ustar = 0.;
     theta = 30.;
     Hmax = 0.5;
     mu = 1.5;
     fpSlope = 0.;
     valleyWallSlp = 0.;
     fpWidth = 0.;
     bankHeight = 3.0;
     chSinu = 0.;
     topW = 0.;
     for (int i=0; i<3; i++)
     {
         flow_area[i] = 0;
         flow_perim[i] = 0;
     }
     hydRadius = 0;
     centr = 0.;
     k_mean = 0.;
     eci = 0.;
     critdepth = 0.;
     rough = 0.;
     omega = 0.;
     Tbed = 20.;
     Tbank = 20.;
     Qb_cap = 0.2;
     comp_D = 0.005;
     K = 0;
     deltaW = 0;
}

void NodeXSObject::xsArea()
{   /* Update X-Section area at a node
     */

    double theta_rad = theta * PI / 180;               // theta is always in degrees
    double ovFp = 0.;                                 // Overtopping elevation, above topmost floodplain height
    double ovBank = 0.;                               // Overtopping elevation, above bank height

    if (bankHeight > Hmax)
        b2b = width + 2 * ( bankHeight - Hmax) / tan( theta_rad );    // Bank-to-bank width (top of in-channel flow section)
    else
        b2b = width;                                  // Case where channel is a rectangle

    double topFp = bankHeight + 1.5;                  // fpSlope = 1:28.5 =~ 2 deg; assume Fp has 1.5m elevation
                                                      // Elevation at point where floodplain meets valley wall
    if (depth > topFp)                                // W.s.l topping floodplain, i.e. wall-to-wall across valley
    {
        ovFp = depth - topFp;
        ovBank = 1.5;

        flow_area[0] = b2b * bankHeight - pow ( ( bankHeight - Hmax ), 2 ) / tan( theta_rad ) +       // Lower trapezoidal portion
                ( ovBank + ovFp ) * b2b;                                   // Upper 'between-bank' flow (both ovBank & ovFp)
        flow_area[1] = 0.5 * ( ovBank * fpSlope * 1.5 ) + 0.5 * ( ovBank * 1.5 ) +      // Over-bank contribution
                ( ovFp * ( fpWidth - b2b) )  + (ovFp * ovFp / valleyWallSlp );          // Over-floodplain contribution
    }

    else if (depth > bankHeight)                            // Is w.s.l. over-bank?
    {
        ovBank = depth - bankHeight;

        flow_area[0] = b2b * bankHeight - pow ( ( bankHeight - Hmax ), 2 ) / tan( theta_rad ) +     // Lower trapezoidal portion
                ovBank * ( b2b + 0.5 * ovBank );            // Upper channel flow, plus wedge against valley wall
        flow_area[1] = 0.5 * ovBank * ovBank * fpSlope;
    }

    else                                                     // w.s.l. is within banks.
    {
        if ( depth <= ( bankHeight - Hmax ) )                 // w.s.l. is below sloping bottom edges
            flow_area[0] = width * depth + pow ( depth, 2 ) / tan( theta_rad );
        else
            flow_area[0] = b2b * depth - pow ( ( bankHeight - Hmax ), 2 ) / tan( theta_rad );

        flow_area[1] = 0;
    }

    flow_area[2] = flow_area[0] + flow_area[1];
}

void NodeXSObject::xsPerim()
{                 // Perimenter at a single node: This could be merged with Area, above.

    double theta_rad = theta * PI / 180;
    double ovFp = 0.;            // Overtopping elevation, above topmost floodplain height
    double ovBank = 0.;          // Overtopping elevation, above bank height
    double b2b = width + (2 * ( bankHeight - Hmax) / tan( theta_rad ));    // Bank-to-bank width (top of in-channel flow section)
    double topFp = bankHeight + 1.5;
                                                                     // Elevation at point where floodplain meets valley wall
    if (depth > topFp)                                               // W.s.l topping floodplain, i.e. wall-to-wall across valley
    {
        ovFp = depth - topFp;
        ovBank = 1.5;

        flow_perim[0] = width + 2 * Hmax + 2 * ( bankHeight - Hmax ) / tan( theta_rad );       // Trapezoidal portion
        flow_perim[1] = ovBank * ( fpSlope + 1.4142 ) + fpWidth -                      // Above bank top
                ( fpSlope * ovBank + b2b + ovBank + 2 * ovFp / valleyWallSlp );
    }

    else if (depth > bankHeight)                                 // W.s.l. is over-bank
    {
        ovBank = depth - bankHeight;

        flow_perim[0] = width + 2 * Hmax + 2 * ( bankHeight - Hmax ) / tan( theta_rad );      // Trapezoidal portion
        flow_perim[1] = ovBank * ( fpSlope + 1.4142 );           // Above bank top
    }

    else                                                                // W.s.l. is within banks.
    {
        if ( depth <= ( bankHeight - Hmax ) )                     // w.s.l. is above sloping bottom edges
            flow_perim[0] = width + 2 * depth / sin ( theta_rad );
        else
            flow_perim[0] = width + 2 * ( bankHeight - Hmax ) / sin ( theta_rad ) + 2 * ( depth - (bankHeight - Hmax) );
        flow_perim[1] = 0;
    }

    flow_perim[2] = flow_perim[0] + flow_perim[1];
    hydRadius = flow_area[2] / flow_perim[2];
}

void NodeXSObject::xsCentr()
{
    // Compute centroid of flow
    double theta_rad = theta * PI / 180;
    double ovBank = 0.;                                                     // Overtopping elevation, above bank height
    double b2b = width + (2 * ( bankHeight - Hmax) / tan( theta_rad ));     // Bank-to-bank width (top of trapezoid)
    double topFp = bankHeight + 1.5;
                                                                            // Elevation at point where floodplain meets valley wall
    /* The formula for a trapezoid with base (a), top width (b),
       and height (h) - arbitrary side slope length - is:

                      / 2a + b  \
           (h/3)  *  | --------  |
                      \  a + b  /                             */

    if (depth > topFp)
    {
        topW = fpWidth;
    }
    else if ( depth > bankHeight )
    {
        ovBank = depth - bankHeight;
        topW = b2b + ovBank * (valleyWallSlp + fpSlope);
    }
    else
    {
            if ( depth < ( bankHeight - Hmax ) )                     // w.s.l. is above sloping bottom edges
                topW = width + 2 * depth / tan ( theta );
            else
                topW = width + 2 * ( bankHeight - Hmax ) / tan ( theta );
    }

    centr = (depth / 3) * ( (2 * width + topW ) / (width + topW) );         // Slightly inaccurate... 3-part approach would be better

    // NEED TO DIVIDE BY TOTAL AREA (e.g. n.xs_area()) AFTER GETTING THIS ARRAY (?)

}

void NodeXSObject::xsECI(NodeGSDObject F)
{

    // The energy coefficient is the ratio of the true kinetic-energy flow rate
    // to the flow rate computed using the average velocity.
    double D_50;

    F.norm_frac();
    F.dg_and_std();                                    // Update grain size statistics

    D_50 = pow( 2, F.dsg ) / 1000.;
    rough = 2 * D_50 * pow( F.stdv, 1.28 );              // roughness height, ks, 2*D90
    if (rough <= 0)         // indicates problems with previous F calcs
        rough = 0.01;
    // N_m = 0.0474 * pow(D_50, 0.1667);                                // Manning's n, as per Dingman (2009) 6.43b, p.250
    // Keulegan Model
    omega = 1 / ( 2.5 * log( 11.0 * ( depth / rough ) ) );                  // Revised 15/03/19: Parker (1991), Dingman 6.25, p.224
    // Ferguson Model
    //omega = pow( pow( 6.5, 2 ) + pow ( 2.5, 2 ) * pow( depth / rough, 1.667 ), 0.5) /
    //            6.5 * 2.5 * (depth / rough);

    double K_ch = flow_area[0] * sqrt( 9.81 * depth ) / omega;              // Dingman, (2009) 8B2.3C, p.300
    double K_fp = 0;
    k_mean = 0;
    double ovBank = depth - bankHeight;

    if (ovBank > 0)
    {
        K_fp = flow_area[1] * sqrt( 9.81 * ovBank * 0.5 ) / omega;   // Depth halfway across the floodplain. Resistance should perhaps be lower for floodplains?
        k_mean = K_ch + K_fp;
        eci = ( pow(K_ch,3) / pow(flow_area[0],2) + pow(K_fp,3) / pow(flow_area[1],2) ) /
                ( pow(k_mean,3) / pow(flow_area[2],2) );                   // Dingman, (2009) 8B2.4, Chaudhry (2nd ed) 4-41
    }
    else
    {
        eci = 1;
        k_mean = K_ch;
    }
}

void NodeXSObject::xsStressTerms(NodeGSDObject F, double bedSlope)
{
    // Compute stresses, transport capacity
    double X;                      // tau_bed / tau_ref for Wilcock equation
    double SFbank = 0;
    double D50 = pow( 2, F.dsg ) / 1000.;
    double theta_rad = theta * PI / 180.;
    double totstress;

    ustar = sqrt( 9.81 * depth * bedSlope );
    velocity = 1 / omega * ustar;

    // use the equations from Knight and others to partition stress
    SFbank = pow( 10.0, ( -1.4026 * log10( width /
            ( flow_perim[2] - width ) + 1.5 ) + 0.3516 ) );    // partioning equation, M&Q93 Eqn.8, E&M04 Eqn.2
    totstress = G * RHO * depth * bedSlope;

    Tbed =  totstress * (1 - SFbank / 100) *
            ( b2b / (2. * width) + 0.5 );           // bed_str = stress acting on the bed, M&Q93 Eqn.10, E&M04 Eqn.4
    Tbank =  totstress * SFbank *
            ( b2b + width ) * sin( theta_rad ) / (4 * depth );

    // estimate the largest grain that the flow can move
    comp_D = Tbed / (0.02 * G * RHO * Gs );

    // estimate the division between key stones and bed material load
    //   (this corresponds to the approximate limit of full mobility)
    K = Tbed / (0.04 * G * RHO * Gs);
}

void NodeXSObject::xsWilcockTransport(NodeGSDObject F){
    // use Wilcock and Crowe to estimate the sediment transport rate

    unsigned int j, k, ngsz, nlith;
    double taussrg;                 // Wilcock - reference (median) shear
    double b;                       // b exponent for each size fraction
    double arg;                     // decision for G
    double phisgo;
    double dj;                      // grain size;
    double ds50;
    double specWt;                  // submerged specific weight of gravel
    double a0;
    double Wwc;                     // Wi* from Wilcock Crowe
    double FGSum;
    vector<double> ktot, ktotn;
    NodeGSDObject fpp;                         // Bedload temp item

    ngsz = F.psi.size() - 2;
    nlith = F.abrasion.size();
    ktot.resize(ngsz);
    ktotn.resize(ngsz);

    specWt = 0.65; //(2650 - 1000) / 1000 - 1.;

    for ( j = 0; j < ngsz; j++ )                        // iterate grain size
        for ( k = 0; k < nlith; k++ )                   // iterate lithology
            fpp.pct[k][j] = F.pct[k][j];                // temp bl is extracted from the surface layer

    fpp.norm_frac();                                       // Normalize f fractions
    fpp.dg_and_std();

    taussrg = 0.021 + 0.015 * exp( -20 * fpp.sand_pct );
    phisgo = ( ( ustar * ustar ) / specWt / 9.81 / (pow( 2, fpp.dsg ) / 1000)) / taussrg;
    FGSum = 1e-10;
    Wwc = 0.;

    for ( j = 0; j < ngsz; j++ )
    {
        ktot[j] = 0;
        a0 = ( 0.5 * ( fpp.psi[j] + fpp.psi[j+1] ) );
        ds50 = pow( 2.0, fpp.dsg ) / 1000;
        dj = pow( 2.0, a0 ) / 1000;
        b = 0.67 / (1 + exp( 1.5 - ( dj / ds50 ) ) );    // Wilcock eqn. (4)
        arg = phisgo * pow( ( dj / ds50 ), -b );

        if (arg < 1.35)
            Wwc = 0.002 * pow( arg, 7.5 );                 // eqn.7a
        else
            Wwc = 14 * pow( ( 1 - 0.894 / sqrt(arg) ), 4.5 );  //eqn. 7b

        for ( k = 0; k < nlith; k++ )
        {
            fpp.pct[k][j] *= Wwc;
            ktot[j] += fpp.pct[k][j];
        }

        FGSum += ktot[j];
    }

    if (FGSum > 0)
        Qb_cap = FGSum * pow( ustar, 3 ) / specWt / 9.81 * width;
    else
        Qb_cap = 0.0;

}

TS_Object::TS_Object()
{

    date_time.setDate(2000, 1, 1);
    date_time.setTime(12, 0, 0);
    Q = 0.;
    Coord = 0;
    GRP = 1;

}

RiverProfile::RiverProfile(XMLElement* params_root)
{

    NodeXSObject tmp;       // to initialize RiverXS
    nnodes = 0;
    npts = 0;
    dt = 0;
    writeInterval = 100;
    dx = 0.0;
    ngsz = 0;
    nlith = 0;
    ngrp = 0;
    ngsz = 0;
    nlith = 0;
    ngrp = 0;
    nlayer = 0;
    poro = 0.0;
    default_la = 0.0;
    layer = 5.0;

    RiverXS.push_back(tmp);

    // TODO: should start and end time be parameters or read from ui??
    cTime.setDate(2002, 12, 5);
    cTime.setTime(12, 0, 0);
    startTime.setDate(2002, 12, 5);
    startTime.setTime(12, 0, 0);
    endTime.setDate(2002, 12, 5);
    endTime.setTime(12, 0, 0);

    counter = 0;
    yearCounter = 0;        // Counter that resets every 5 days
			// e.g. 5d * 24hr * 60 * 60 / ( dt )

    for (int i = 0; i < 5; i++)
	    N.push_back(0);                   // Substrate shift matrix
    for (int i = 0; i < 10; i++)
	    rand_nums.push_back(0);
                                                 // Random tweak variables are based on logarithmic (e) scaled values
                                                 // Augment the rate of tributary Qs, Qw inputs
    tweakArray = hydroGraph();                        // A gamma-distribution that simulates hydrograph form
    qsTweak = 1;                                      //rand_nums[1] * 1.5 + 0.5;        // qs between 0.5 and 2
    qwTweak = 1;                                      // Hydrograph multiplier
    substrDial = 0;                                   // rand_nums[3]  * 3.8 - 1.9;      // Positive (up to +2) makes finer mix, negative (down to -2) coarsens all grain groups
    feedQw =  1;                                      // rand_nums[4]  * 0.5 + 0.75;     // between 0.75 and 1.25
    feedQs =  1;                                      // rand_nums[5] + 0.5;                     // between 0.5 and 1.5
    HmaxTweak = 1;                                    // See line ~750ff
    randAbr = 0.00001;                               // between 10^-4 and 10^-7

    // Set up substrate shift matrix

    if (substrDial > 0 && substrDial < 1)
    {
         N[2] = 1 - substrDial;
         N[3] = substrDial;
    }
    if (substrDial >= 1 && substrDial < 2)
    {
         N[3] = 1 - ( substrDial - 1 );
         N[4] = ( substrDial - 1 );
    }
    if (substrDial >= 2) N[4] = 1;
    if (substrDial == 0) N[2] = 1;
    if (substrDial < 0 && substrDial > -1)
    {
        substrDial = abs(substrDial);
        N[1] = abs(substrDial);
        N[2] = 1 - abs(substrDial);
    }
    if (substrDial <= -1 && substrDial > -2)
    {
        N[0] = abs( substrDial + 1 );
        N[1] = 1 - abs( substrDial + 1 );
    }
    if (substrDial <= -2) N[0] = 1;

    if ( ( N[0] + N[1] + N[2] + N[3] + N[4] ) > 1)
       cout << "Interpolation Array is over 1.0";

    initData(params_root);

    outputFile = "RunResults.txt";         //  TXT file to write results

}

vector<double> RiverProfile::hydroGraph()
{
    /* This routine creates a hydrograph, based on the gamma distribution,
       meant to simulate the range of flow experienced over the course of
       1 year.
    */
  unsigned int i = 0;

  double max_flow = 1.6;  // Up to 1.6 * 50 = 80 m3/s
  double min_flow = 0.8;
  double elems = 30240;   // 3.5 days, at dt=10 secs
  double alpha = 4;
  double beta = 5;
  double delta = 0.0052;
  double factor = 0;
  vector<double> x, xx, fac;

  for (i = 0; i < elems; ++i)
  {
    x.push_back(0);
    xx.push_back(0);
    fac.push_back(0);
  }

  x[0] = -1.5;
  for (i = 1; i < elems; ++i)
  {
    x[i] = x[i-1] + delta;
    xx[i] = pow(10,x[i]);
    factor = alpha*log(beta)-gammln2(alpha);
    fac[i] = exp(-beta * xx[i] + ( alpha - 1. ) * log( xx[i] ) + factor);
    if (fac[i] < 0.001)
      fac[i] = 0.001;
    fac[i] = (fac[i]) * (max_flow - min_flow) + min_flow;
    // std::cout << x[i] << ":\t" << fac[i] << endl;
  }
  fac[0] = fac[1];

  return fac;
}

void RiverProfile::initData(XMLElement* params_root)
{
    // get params element
    XMLElement *params = params_root->FirstChildElement("PARAMS");
    if (params == NULL) {
        throw std::string("Error getting PARAMS element from XML file");
    }

    NodeGSDObject tmp;
    vector < NodeGSDObject > tmp2;

    nnodes = getIntValue(params, "NNODES");

    // Allocate vectors
    xx.resize(nnodes);
    eta.resize(nnodes);
    algrp.resize(nnodes);
    stgrp.resize(nnodes);
    bedrock.resize(nnodes);
    RiverXS.resize(nnodes);

    layer = getDoubleValue(params, "LAYER");

    toplayer.assign(nnodes, layer);               // Thickness of the top storage layer; starts at 5 and erodes down

    default_la = getDoubleValue(params, "LA");
    la.assign(nnodes, default_la);                // Default active layer thickness

    nlayer = getIntValue(params, "NLAYER");
    ntop.assign(nnodes, nlayer-12);                // Indicates # of layers remaining, below current (couple of layers left for aggradation)

    poro = getDoubleValue(params, "PORO");

    for (unsigned int i = 0; i < nlayer; i++)                  // Init storedf stratigraphy matrix
        tmp2.push_back(tmp);

    for (unsigned int i = 0; i < nnodes; i++)
    {
        storedf.push_back(tmp2);
        F.push_back(tmp);
    }

    ngsz = getIntValue(params, "NGSZ");

    nlith = getIntValue(params, "NLITH");

    ngrp = getIntValue(params, "NGRP");

    for (unsigned int i = 0; i < ngrp; i++)
        grp.push_back(tmp);

    getGSDLibrary(params_root);

    for (unsigned int i = 0; i < nnodes; i++)
    {
        RiverXS[i].depth = 1.5;
        RiverXS[i].width = 30.;
        RiverXS[i].wsl = eta[i] + RiverXS[i].depth;
        RiverXS[i].velocity = 0.;
        RiverXS[i].node = i;
        RiverXS[i].fpSlope = 28.5;
        RiverXS[i].valleyWallSlp = 0.6;
        RiverXS[i].chSinu = 1.05;
        algrp[i] = 1;
        toplayer[i] = 5;
        stgrp[i] = 1;
        if (stgrp[i] == 0)                           // This is used for knickpoint control
            bedrock[i] = eta[i];
        else
            bedrock[i] = eta[i] - ( nlayer * layer );
    }

    // TODO: NPTS not in xml file but was in the old dat file (is it the same as NNODES??)
    npts = nnodes;

    getLongProfile(params_root);

    getStratigraphy(params_root);

    dx = xx[1]-xx[0];                       // Assume uniform grid
    dt = 10;
    writeInterval = 100;

}

void RiverProfile::getGSDLibrary(XMLElement* params_root)
{
    // loop over lithologies
    for (unsigned int lithCount = 0; lithCount < nlith; lithCount++)
    {
        // name of this element
        std::stringstream ss;
        ss << "LITH" << lithCount + 1;
        std::string lithName = ss.str();

        // get the element
        XMLElement *lithElem = params_root->FirstChildElement(lithName.c_str());
        if (lithElem == NULL) {
            std::ostringstream oss;
            oss << "Error getting " << lithName << " element";
            throw oss.str();
        }

        // loop over groups
        int grpCount = 0;
        for (XMLElement* e = lithElem->FirstChildElement("GRP"); e != NULL; e = e->NextSiblingElement("GRP")) {
            // loop over psi
            for (int gsCount = 0; gsCount < ngsz; gsCount++) {
                // name of this psi element
                // CHECK: do they alway start -3 to 9
                std::stringstream ss_psi;
                ss_psi << "PSI_" << gsCount - 3;
                std::string psiName = ss_psi.str();
                
                // get the element
                XMLElement* psiElem = e->FirstChildElement(psiName.c_str());
                if (psiElem == NULL) {
                    std::ostringstream oss;
                    oss << "Error getting element " << psiName << " for " << lithName;
                    throw oss.str();
                }

                // get the value
                double tmpval;
                if (psiElem->QueryDoubleText(&tmpval)) {
                    std::cerr << "Error getting value for " << lithName << " - " << psiName << std::endl;
                }
                grp[grpCount].pct[lithCount][gsCount] = tmpval;

                // TODO: get other things, i.e. ABR, RHOS
            }

            grp[grpCount].pct[lithCount][13] = 100;    // Extra grain size slots - temporary fix.
            grp[grpCount].pct[lithCount][14] = 100;
            grp[grpCount].abrasion[lithCount] = getDoubleValue(e, "ABR");
            grp[grpCount].density[lithCount] = getDoubleValue(e, "RHOS");
            grpCount++;
        }
        if (grpCount != ngrp) {
            std::ostringstream oss;
            oss << "Wrong number of groups for " << lithName;
            throw oss.str();
        }
    }

    // Take cumulative data and turn it into normalized fractions
    for (int grpCount = 0; grpCount < ngrp; grpCount++)
        for (int gsCount = ngsz+1; gsCount > 0; gsCount--)
        {
            grp[grpCount].pct[0][gsCount] -= grp[grpCount].pct[0][gsCount - 1];
            grp[grpCount].pct[1][gsCount] -= grp[grpCount].pct[1][gsCount - 1];
            grp[grpCount].pct[2][gsCount] -= grp[grpCount].pct[2][gsCount - 1];
        }
        
    // Carry out substrate shift; for randomization work
    for (int grpCount = 0; grpCount < ngrp; grpCount++)
    {
        NodeGSDObject qtemp;

        for (int gsCount = 0; gsCount < ngsz; gsCount++)
        {
            for (int lithCount = 0; lithCount < nlith; lithCount++)                           // last term is a sand content addition
            {
                if ( gsCount == 0 )
                    qtemp.pct[lithCount][gsCount] = N[2] * grp[grpCount].pct[gsCount][lithCount] + N[3] * grp[grpCount].pct[lithCount][gsCount+1]
                        + N[4] * grp[grpCount].pct[lithCount][gsCount+2];
                else if ( gsCount == 1 )
                    qtemp.pct[lithCount][gsCount] = N[1]*grp[grpCount].pct[lithCount][gsCount-1]
                        + N[2]*grp[grpCount].pct[lithCount][gsCount] + N[3]*grp[grpCount].pct[lithCount][gsCount+1]
                        + N[4]*grp[grpCount].pct[lithCount][gsCount+2];
                else if ( gsCount == ngsz-2 )
                    qtemp.pct[lithCount][gsCount]= N[0]*grp[grpCount].pct[lithCount][gsCount-2]+ N[1]*grp[grpCount].pct[lithCount][gsCount-1]
                        + N[2]*grp[grpCount].pct[lithCount][gsCount] + N[3]*grp[grpCount].pct[lithCount][gsCount+1];
                else if ( gsCount == ngsz-1 )
                    qtemp.pct[lithCount][gsCount]= N[0]*grp[grpCount].pct[lithCount][gsCount-2]+ N[1]*grp[grpCount].pct[lithCount][gsCount-1]
                        + N[2]*grp[grpCount].pct[lithCount][gsCount];
                else
                    qtemp.pct[lithCount][gsCount]= N[0]*grp[grpCount].pct[lithCount][gsCount-2]+ N[1]*grp[grpCount].pct[lithCount][gsCount-1]
                        + N[2]*grp[grpCount].pct[lithCount][gsCount] + N[3]*grp[grpCount].pct[lithCount][gsCount+1]
                       + N[4]*grp[grpCount].pct[lithCount][gsCount+2];
            }
        }

        qtemp.norm_frac();
        for (int gsCount = 0; gsCount < ngsz; gsCount++)
            for (int lithCount = 0; lithCount < nlith; lithCount++)
                grp[grpCount].pct[lithCount][gsCount] = qtemp.pct[lithCount][gsCount];
        grp[grpCount].dg_and_std();
    }
}

void RiverProfile::getLongProfile(XMLElement* params_root)
{
    // get the "profile" element
    XMLElement *profileElem = params_root->FirstChildElement("profile");
    if (profileElem == NULL) {
        throw std::string("Error getting profile element from XML file");
    }

    // loop over entries
    int m = 0;
    for (XMLElement* e = profileElem->FirstChildElement("XX"); e != NULL; e = e->NextSiblingElement("XX")) {
        // xx
        if (e->QueryDoubleAttribute("X", &xx[m])) {
            throw std::string("Error getting X attribute from XX profile element");
        }

        eta[m] = getDoubleValue(e, "ETA");

        bedrock[m] = getDoubleValue(e, "BEDROCK");
        if (bedrock[m] > eta[m])
            bedrock[m] = eta[m];       // bedrock must be at, or lower than, initial bed

        RiverXS[m].width = getDoubleValue(e, "WIDTH");

        RiverXS[m].chSinu = getDoubleValue(e, "SINU");

        RiverXS[m].fpWidth = getDoubleValue(e, "FPWIDTH") * RiverXS[m].width;

/*        if ( HmaxTweak < 0.5 )
            RiverXS[m].Hmax = atof(token[4]) + ( HmaxTweak * 2 - 0.5 );    // Add height in the range [-0.5 to +0.5]
        else
            RiverXS[m].Hmax = (HmaxTweak - 0.5) * 3.5 + 0.75; */             // Uniform range from 0.75 to 2.5
        RiverXS[m].Hmax = getDoubleValue(e, "HMAX");
        RiverXS[m].bankHeight = RiverXS[m].Hmax + 1;  // initial guess

        RiverXS[m].theta = getDoubleValue(e, "THETA");

        algrp[m] = getIntValue(e, "ALGRP") - 1;
        stgrp[m] = getIntValue(e, "STGRP") - 1;

        m++;
    }
}

void RiverProfile::getStratigraphy(XMLElement* params_root)
{
    std::ostringstream layername;
    int node = 0;

    // get the "stratigraphy" element
    XMLElement *stratElem = params_root->FirstChildElement("stratigraphy");
    if (stratElem == NULL) {
        //throw std::string("Error getting stratigraphy element from XML file");
        for (int z = 1; z < (nlayer + 1); z++){
            int st_grp = stgrp[node];      // Build stratigraphy from subsurface information
            for (int j = 0; j < ngsz; j++) {
                for (int k = 0; k < nlith; k++) {
                    storedf[node][z-1].pct[k][j] = grp[st_grp-1].pct[k][j];
                    if (j == 0){
                        storedf[node][z-1].abrasion[k] = grp[st_grp-1].abrasion[k];
                        storedf[node][z-1].density[k] = grp[st_grp-1].density[k];
                    }
                }
            }
        }
    }
    else
    {   // Or, if stratigraphy does exist in the xml file, then read it in
    for (XMLElement* e = stratElem->FirstChildElement("XXX"); e != NULL; e = e->NextSiblingElement("XXX")) {
        if (e->QueryDoubleAttribute("X1", &xx[node])) {
            throw std::string("Error getting X attribute from X1 stratigraphy element");
        }
        for (int z = 1; z < 31; z++){
            layername << "layer" << std::setfill('0') << std::setw(2) << ( z );         // Get 'layer01', 'layer02', etc.
            int st_grp = getIntValue(e, layername.str().c_str());
            for (int j = 0; j < ngsz; j++) {
                for (int k = 0; k < nlith; k++) {
                    storedf[node][z-1].pct[k][j] = grp[st_grp-1].pct[k][j];
                    if (j == 0){
                        storedf[node][z-1].abrasion[k] = grp[st_grp-1].abrasion[k];
                        storedf[node][z-1].density[k] = grp[st_grp-1].density[k];
                    }
                }
            }
            layername.str("");                  // clear contents of ostringstream object
            }
        node++;
        }
    }

    for (int i = 0; i < nnodes; i++)            // Populate initial active layer bed GSD
    {
        for (int j = 0; j < ngsz; j++)
            for (int k = 0; k < nlith; k++)
                F[i].pct[k][j] = grp[algrp[i]].pct[k][j];
        F[i].norm_frac();
        F[i].dg_and_std();
        F[i].abrasion[0] = randAbr;
        F[i].abrasion[1] = randAbr;
        F[i].abrasion[2] = randAbr;
    }
}
