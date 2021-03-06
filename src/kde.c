/*************************************************
  @file: kde.c

 Perform Kernel Estimation of the density and distribution function using 
 gaussian kernel for real values gamma kernel for non-negative values. 
 Also it uses the same data as the grid values to compute the range. If not
 bandwidth is provide it will compute a rule-of-thumb estimate assuming the 
 data arise from a gamma and gaussian.
Also it may be possible to remove the dependence from GSL to Rmath. I think GSL
 is better but Rmath is faster.
 
 Require: GSL library
 
Author: 
Israel Almodóvar-Rivera, PhD
Department of Biostatistics and Epidemiology
University of Puerto Rico
Medical Science Campus
israel.almodovar@upr.edu
 
Created in January 2018
Updated January 2020
**************************************************************/

#include <gsl/gsl_math.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_sort.h> 
#include <gsl/gsl_statistics.h> 
#include <gsl/gsl_cdf.h>

#include "array.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void bandwidth_mise_cdf(double (*x), int (*n), double (*bw))
{

  int n1;
  n1 = (*n);
  
  double a = 0.0, b = 0.0, mean  = gsl_stats_mean(x,1,(size_t) n1);
  double t1 = 0.0, bb = 0.0;
  
  b = gsl_stats_variance(x, 1, (size_t) n1)/mean;
  a = mean/b;
  
  t1 = (a )/(a + 4.0); 
  bb =  6.1311 * b * pow(t1*t1,1.0/3.0) * pow((double)n1,-2.0/3.0);

  (*bw) = bb;
}


void bandwidth_mise_gaussian(double (*x), int (*n),double (*bw))
{
  int n1;
  n1 = (*n);
  n1 = (size_t) n1;

  
  double IQR = 0.0,b = 0.0;

  gsl_sort(x,1, n1);
  
  IQR = gsl_stats_quantile_from_sorted_data (x, 1, n1, 0.75) - gsl_stats_quantile_from_sorted_data (x, 1, n1, 0.25);

  b = 3.572041 * pow((double) n1, -1.0/3.0) * MIN(gsl_stats_sd(x,1, n1), IQR/1.349) ;

  (*bw) = b;
	
}

void gaussian_kcdf(double (*x), int (*n), double (*xgrid), int (*m),double (*bw), double (*Fhat))
{

  int i, j;
     
  for(j = 0; j < (*m); j++){
    Fhat[j] = 0.;
    for(i = 0; i < (*n); i++)
      Fhat[j] += gsl_cdf_ugaussian_P((xgrid[j]-x[i])/(*bw));//pnorm((xgrid[j]-x[i])/(*bw),0,1,1,0);
    
    Fhat[j] /= (*n);
  }

}


void bandwidth_mise_pdf_gaussian(double (*x), int (*n), double (*bw))
{
  int n1;
  double IQR=0.0, b = 0.0;

  n1 = (*n);

  gsl_sort(x, 1, n1);
    
  IQR = gsl_stats_quantile_from_sorted_data (x, 1, (size_t)n1, 0.75) - gsl_stats_quantile_from_sorted_data (x, 1, (size_t)n1, 0.25);
  
  b = 1.059 * pow((double) n1, -0.20) * MIN(gsl_stats_sd(x,1,(size_t)n1), IQR/1.349);

  (*bw) = b;
}


void gaussian_kpdf(double *x, int (*n), double (*xgrid), int (*m), double (*bw), double *fhat){

  int i, j, n1;
  double b;
  
  b = (*bw);
  n1 = (*n);
  
  for(j = 0; j < (*m); j++){
    fhat[j] = 0.;
    for(i = 0; i < (*n); i++)
      fhat[j] += gsl_cdf_ugaussian_P((xgrid[j]-x[i])/b);//pnorm((xgrid[j]-x[i])/b,0,1.0,1,0);
    
    fhat[j] /= (double)(b*n1);
  }
}
/***
 Construct gamma kernel estimator
 ***/
struct f_params {
  double y;
  double b;
};

double f (double x, void * p) {

  struct f_params * params = (struct f_params *) p;
  double y = (params -> y);
  double b = (params -> b);
  double f = gsl_ran_gamma_pdf(y, x/b + 1.0, b);
  //double f = dgamma(y,x/b+1.0,b,0);;
  
  return f;
}

void gamma_kcdf(double (*x), int (*n), double (*xgrid), int (*m),double (*bw), double (*Fhat))
{

  int i, j, n1;
  struct f_params theta;
  double b, result, error, y;

  n1 = (*n);
  
  gsl_integration_workspace * w = gsl_integration_workspace_alloc (2*n1);
    
  b = (*bw);
  gsl_function F;
  F.function = &f;
  F.params = &theta;
  theta.b = b;
  for(j = 0; j < (*m); j++) {
    Fhat[j] = 0.;
    for(i  = 0; i < n1; i++) {
      y = x[i];
      theta.y = y;
      gsl_integration_qags(&F, 0, xgrid[j], 0, 1e-10, n1, w, &result, &error);
      Fhat[j] += result;
    }
    Fhat[j] /= n1;
}
  (*bw) = b;
  gsl_integration_workspace_free (w);
}
/***
Rule-of-thumb bandwidth for gamma kernel. Obtain by minimizing the Mean Integrated Squared Error (MISE), 
it assume that the true pdf can be explain from a gamma(alpha,beta). beta is in term of the scale.
****/
void bandwidth_mise_gamma_pdf(double *x, int (*n), double (*bw))
{

  int n1;
  double mean ,alpha, beta, eval =0., b;
  n1 = (*n);
  
  mean = gsl_stats_mean(x, 1, n1); 
  beta = gsl_stats_variance(x, 1, n1)/mean;
  alpha = mean/beta;
  eval = (alpha-1.5)/(alpha*n1);
  b = 1.12915 * pow(eval, 0.4)*beta;
  (*bw) = b;
}

/*******************************************
Gamma kernel density estimator from Chen (2000).
*****************************************/
void gamma_kpdf(double (*x), int (*n), double (*xgrid), int (*m),double (*bw), double (*fhat))
{
  int i, j, n1;
  double b;

  n1 = (*n);
  
  b = (*bw);

  for(j = 0; j < (*m); j++){
    for(i = 0; i < n1; i++)
        fhat[j] += gsl_ran_gamma_pdf(x[i], xgrid[j]/b + 1.0, b);
    //fhat[j] += dgamma(x[i],xgrid[j]/b + 1.0,b,0); This can be use if Rmath is loaded

    fhat[j]/= n1; 
  }

}
/*****************************************************************
Gamma kernel density estimator with inverse roles Kim (2013). 
note that this kernel inverse its point 
with the observations and it does integrate to 1.
******************************************************************/

void gamma_kpdf_inv(double (*x), int (*n), double (*xgrid), int (*m),double (*bw), double (*fhat))
{
  int i, j;
  double b;
  
  gsl_sort(xgrid,1,(size_t) (*m));
  
  b = (*bw);

  for(j = 0; j < (*m); j++){
    for(i = 0; i < (*n); i++)
      fhat[j] += gsl_ran_gamma_pdf(xgrid[j], x[i]/b + 1.0, b); //dgamma(xgrid[j],x[i]/b + 1.0,b,0);

    fhat[j]/= (*n); 
  }

}
/***********************************************************************
Reciprocal Inverse Gaussian (RIG) kernel estimator from Scaillet 2004.
***********************************************************************/
void RIG_kcdf(double (*x), int (*n), double(*xgrid),int (*m),double (*bw), double (*Fhat))
{
  int i, j;
  double *xb, b, Phi = 0.0;
  
  MAKE_VECTOR(xb, (*n));

  b = (*bw);

  for(i = 0; i < (*n); i++)
    xb[i] = pow(x[i]*b, -0.5);

  for(i = 0; i < (*n); i++)
    Phi += gsl_cdf_ugaussian_P((x[i]+b)*xb[i]);//pnorm((x[i]+b)*xb[i],0,1,1,0);

  Phi /= (*n);
  
  for(j = 0; j < (*m); j++){   
    Fhat[j] = 0.0;
    for(i = 0; i < (*n); i++)
      Fhat[j] += gsl_cdf_ugaussian_P((x[i]+b-xgrid[j])*xb[i]);//pnorm((x[i]+b-xgrid[j])*xb[i],0,1,1,0);

    Fhat[j] /= (*n);
    Fhat[j] = Phi - Fhat[j];
  }

  FREE_VECTOR(xb);
}

/******************************************************************
    This second gamma kernel is similar as Chen 2000 
    but invert the roles Kim (2013) of the point estimation with the data. 
    Asympotically they behave similarly however this one does
    integrate to one.
*****************************************************************/

void gamma_kcdf_inv(double (*x), int (*n),  double(*xgrid),int (*m),double (*bw), double (*Fhat))
{
  
  int i, j;
  double b;
  
  b=(*bw);

  for(j = 0; j < (*m); j++){
    Fhat[j] = 0.0;
    for(i = 0; i < (*n); i++)
      Fhat[j] += gsl_cdf_gamma_P(xgrid[j],x[i]/b+1.0,b);//pgamma(xgrid[j], x[i]/b+1.0, b, 1, 0);

    Fhat[j] /= (*n);
  }

  (*bw) = b;

}


