#ifndef YM_LOCAL_C
#define YM_LOCAL_C

#include"../include/macro.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#ifdef OPENMP_MODE
  #include<omp.h>
#endif

#include"../include/function_pointers.h"
#include"../include/gauge_conf.h"
#include"../include/geometry.h"
#include"../include/gparam.h"
#include"../include/random.h"

void real_main(char *in_file)
{
	Gauge_Conf GC;
	//Gauge_Conf help_albert;
	Geometry geo;
	GParam param;

	char name[STD_STRING_LENGTH], aux[STD_STRING_LENGTH];
	int count;
	FILE *datafilep, *chiprimefilep, *topchar_tcorr_filep, *datafilep_top0, *datafilep_top1, *datafilep_top2, *datafilep_top3;
	time_t time1, time2;

	// to disable nested parallelism
	#ifdef OPENMP_MODE
		// omp_set_nested(0); // deprecated
		omp_set_max_active_levels(1); // should do the same as the old omp_set_nested(0)
	#endif

	// read input file
	readinput(in_file, &param);

	// initialize random generator
	initrand(param.d_randseed);

	// open data_file
	init_data_file(&datafilep, &datafilep_top0, &datafilep_top1, &datafilep_top2, &datafilep_top3, &chiprimefilep, &topchar_tcorr_filep, &param);

	// initialize geometry
	init_indexing_lexeo();
	init_geometry(&geo, &param);

	// if measure-only mode is active conf must be read from file => start=2 ignoring the value found in input file
	if (param.d_sample == 0)
	{
		fprintf(stdout, "MEASURE-ONLY MODE: performing measures on configuration read from file %s, no update will be performed\n", param.d_conf_file);
		param.d_start=2;
	}

	// initialize gauge configuration
	init_gauge_conf(&GC, &param);

	// Monte Carlo begins
	time(&time1);
	if (param.d_sample == 0) // no update is done, only measures are performed on read configuration
	{
		perform_measures_localobs_GF(&GC, &geo, &param, datafilep, chiprimefilep, topchar_tcorr_filep, datafilep_top0, datafilep_top1, datafilep_top2, datafilep_top3);
	}
	else
	{
		for(count=0; count < param.d_sample; count++)
		{
			// update conf
			update(&GC, &geo, &param);

			// measure local observables with grandient flow (if cooling, change the routine)
			if(GC.update_index % param.d_measevery == 0 && GC.update_index >= param.d_thermal)
			{
				perform_measures_localobs_GF(&GC, &geo, &param, datafilep, chiprimefilep, topchar_tcorr_filep, datafilep_top0, datafilep_top1, datafilep_top2, datafilep_top3);
			}

			// save configuration for backup
			if(param.d_saveconf_back_every!=0)
			{
				if(GC.update_index % param.d_saveconf_back_every == 0 )
				{
					// simple
					write_conf_on_file(&GC, &param);

					// backup copy
					write_conf_on_file_back(&GC, &param);
				}
			}

			// save configuration for offline analysis
			if(param.d_saveconf_analysis_every!=0)
			{
				if(GC.update_index % param.d_saveconf_analysis_every == 0 )
				{
					strcpy(name, param.d_conf_file);
					strcat(name, "_step_");
					sprintf(aux, "%ld", GC.update_index);
					strcat(name, aux);
					write_conf_on_file_with_name(&GC, &param, name);
				}
			}
		}
	}
	time(&time2);
	//albertos test
	//init_gauge_conf(&help_albert, &param);
	//read_conf_alberto(&help_albert,&geo,&param);

	// Monte Carlo ends

	// close data file
	fclose(datafilep);
	fclose(datafilep_top0);
	fclose(datafilep_top1);
	fclose(datafilep_top2);
	fclose(datafilep_top3);
	if (param.d_chi_prime_meas==1) fclose(chiprimefilep);
	if (param.d_topcharge_tcorr_meas==1) fclose(topchar_tcorr_filep);

	// save configuration
	if(param.d_saveconf_back_every!=0)
	{
		write_conf_on_file(&GC, &param);
	}

	// print simulation details
	print_parameters_local(&param, time1, time2);

	// free gauge configuration
	free_gauge_conf(&GC, &param);

	// free geometry
	free_geometry(&geo, &param);
}

void print_template_input(void)
{
	FILE *fp;
	fp=fopen("template_input.in", "w");

	if(fp==NULL)
	{
		fprintf(stderr, "Error in opening the file template_input.in (%s, %d)\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
  else
	{
		fprintf(fp, "size 4 4 4 4\n");
		fprintf(fp,"\n");
		fprintf(fp,"# twist parameters\n");
		fprintf(fp,"k_twist 0 0 0 1 0 0 # ktwist on the plane 01,02,03,12,13,23");
		fprintf(fp,"\n");
		fprintf(fp, "beta 6.000\n");
		fprintf(fp, "theta 0.000\n");
		fprintf(fp,"\n");
		fprintf(fp, "sample    1\n");
		fprintf(fp, "thermal   0\n");
		fprintf(fp, "overrelax 10\n");
		fprintf(fp, "measevery 1\n");
		fprintf(fp,"\n");
		fprintf(fp, "start                    3  # 0=ordered  1=random  2=from saved configuration 3=cold isted(only for 12 dir)\n");
		fprintf(fp, "saveconf_back_every      10  # if 0 does not save, else save backup configurations every ... updates\n");
		fprintf(fp, "saveconf_analysis_every  10  # if 0 does not save, else save configurations for analysis every ... updates\n");
		fprintf(fp, "\n");
		fprintf(fp, "coolsteps             3  # number of cooling steps to be used\n");
		fprintf(fp, "coolrepeat            5  # number of times 'coolsteps' are repeated\n");
		fprintf(fp, "chi_prime_meas        0  # 1=YES, 0=NO\n");
		fprintf(fp, "topcharge_tcorr_meas  0  # 1=YES, 0=NO\n");
		fprintf(fp,"\n");
		fprintf(fp, "#for gradient flow evolution\n");
		fprintf(fp, "gfstep      0.01    # integration step for gradient flow\n");
		fprintf(fp, "num_gfsteps 1000     # number of integration steps for gradient flow\n");
		fprintf(fp, "gf_meas_each 1000       # compute observables every <gf_meas_each> integration steps during the gradient flow\n");
		fprintf(fp,"\n");
		fprintf(fp, "#output files\n");
		fprintf(fp, "conf_file             conf.dat\n");
		fprintf(fp, "data_file             dati.dat\n");
		fprintf(fp, "chiprime_data_file    chiprime_cool.dat\n");
		fprintf(fp, "topcharge_tcorr_file  topo_tcorr_cool.dat\n");
		fprintf(fp, "log_file              log.dat\n");
		fprintf(fp, "\n");
		fprintf(fp, "randseed 0    #(0=time)\n");
		fclose(fp);
	}
}

int main (int argc, char **argv)
{
	char in_file[STD_STRING_LENGTH];
	if(argc != 2)
	{
		printf("\nPackage %s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		printf("Claudio Bonati %s\n", PACKAGE_BUGREPORT);
		printf("Usage: %s input_file\n\n", argv[0]);

		printf("Compilation details:\n");
		printf("\tN_c (number of colors): %d\n", NCOLOR);
		printf("\tST_dim (space-time dimensionality): %d\n", STDIM);
		printf("\tNum_levels (number of levels): %d\n", NLEVELS);
		printf("\n");
		printf("\tINT_ALIGN: %s\n", QUOTEME(INT_ALIGN));
		printf("\tDOUBLE_ALIGN: %s\n", QUOTEME(DOUBLE_ALIGN));

		#ifdef DEBUG
			printf("\n\tDEBUG mode\n");
		#endif

		#ifdef OPENMP_MODE
			printf("\n\tusing OpenMP with %d threads\n", NTHREADS);
		#endif

		#ifdef THETA_MODE
			printf("\n\tusing imaginary theta\n");
		#endif

		printf("\n");

		#ifdef __INTEL_COMPILER
			printf("\tcompiled with icc\n");
		#elif defined(__clang__)
			printf("\tcompiled with clang\n");
		#elif defined( __GNUC__ )
			printf("\tcompiled with gcc version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
		#endif

		print_template_input();

		return EXIT_SUCCESS;
	}
	else
	{
		if(strlen(argv[1]) >= STD_STRING_LENGTH)
		{
			fprintf(stderr, "File name too long. Increse STD_STRING_LENGTH in include/macro.h\n");
		}
		else
		{
			strcpy(in_file, argv[1]);
		}
	}
	real_main(in_file);
	return EXIT_SUCCESS;
}

#endif
