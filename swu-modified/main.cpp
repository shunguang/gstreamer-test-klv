/*
 * main.cpp
 *
 *  Created on: June 5, 2020
 *      Author: wus1
 */
#include "DataTypes.h"

int test_gst_enc_H264_wo_klv(int argc, char **argv);
int test_gst_klv_enc_min_v1(int argc, char **argv);
int test_gst_klv_dec_v1(int argc, char *argv[]);

void app_usage();
int main(int argc, char* argv[])
{
        int x = 0;
         maven_usage();
        if (argc < 2){
                printf("to few input params\n");
                return -1;
        }


        int flag = atoi( argv[1] );
        switch (flag){
        case  1:
                x=test_gst_klv_enc_min_v1(argc, argv);
                break;
        case 2:
                x=test_gst_enc_H264_wo_klv(argc, argv);
                break;
        case 3:
                x=test_gst_klv_dec_v1(argc, argv);
                break;
        default:
                printf("todo: ");
                break;
        }

        return x;
}

void app_usage()
{
        printf( "test.out intFlag\n");
        printf( "example: \n");
        printf( "test.out 1 -- test_gst_klv_enc_min_v1()\n");
        printf( "test.out 2 -- test_gst_enc_H264_wo_klv()\n");
        printf( "test.out 3 -- test_gst_klv_dec_v1()\n");
}
