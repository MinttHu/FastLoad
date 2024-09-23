#include "auxiliary.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "omp.h"
#include <algorithm>
#include <vector>
using namespace std;


void csr2csc(int nnz,
             int rowA,
             int colA,
             int *csrcolidx,
             int *csrrowptr,
             double *csrval,
             int **cscrowidx,
             int **csccolptr,
             int **nnzpercol,
             double **cscval)
{
    double *cscval_tmp = (double *)malloc(sizeof(double)*nnz);
    memset(cscval_tmp,0,sizeof(double)*(nnz));
    int *csccolptr_tmp = (int *)malloc(sizeof(int)*(colA+1));
    memset(csccolptr_tmp,0,sizeof(int)*(colA+1));
    int *cscrowidx_tmp = (int *)malloc(sizeof(int)*nnz);
    memset(cscrowidx_tmp,0,sizeof(int)*(nnz));
    int *nnzpercol_tmp = (int *)malloc(sizeof(int)*colA);
    memset(nnzpercol_tmp,0,sizeof(int)*colA);
    
    for(int i = 0; i<nnz;i++)
    {
        int j = csrcolidx[i];
        csccolptr_tmp[j]++;
        nnzpercol_tmp[j]++;
    }

    exclusive_scan(csccolptr_tmp,colA+1);
    int nnz_tmp1 = csccolptr_tmp[colA];
    if(nnz_tmp1!= nnz)
    {printf("error1,fail csr2csc, nnz_tmp1 = %d\n", nnz_tmp1);}
    

    for(int i = 0;i<rowA;i++)
    {
        int start = csrrowptr[i];
        int stop = csrrowptr[i+1];
        for(int j = start;j<stop;j++)
        {
            int k = csrcolidx[j];
            int l =csccolptr_tmp[k];
            cscrowidx_tmp[l] = i;
            cscval_tmp[l] = csrval[j];
            csccolptr_tmp[k]++;
        }
    }

    for(int i = colA-1;i>=1;i-- )
    {
        csccolptr_tmp[i] = csccolptr_tmp[i]- nnzpercol_tmp[i];
    }
    csccolptr_tmp[0]=0;

    int nnz_tmp2 = csccolptr_tmp[colA];
    if(nnz_tmp2 != nnz)
    {printf("error2,fail csr2csc, nnz_tmp2 = %d\n",nnz_tmp2);}

    *cscval = cscval_tmp;
    *csccolptr = csccolptr_tmp;
    *cscrowidx = cscrowidx_tmp;
    *nnzpercol = nnzpercol_tmp;
}


void col_sort(int colA,
              int *nnzpercol,
              int *csccolptr,
              int *cscrowidx,
              double *cscval,
              
              int *sortrowidx_tmp,
              double *sortval_tmp,
              int *sortnnz_tmp,
              
              double *x,
              double *sortx,
              
              int nnz)
              //int *forsortedCol,
              //int *forsortedIndice,
              //double *forsortedx,
              //int *fortestsortedidx
{
    vector<sortA> arr(colA);
    for (int i = 0; i < colA; i++) 
    {
        arr[i].sortnnzpercol = nnzpercol[i];
        arr[i].index = i;
    }
    sort(arr.begin(), arr.end(), cmp);

    int *index_tmp = (int *)malloc(sizeof(int)*colA);
    for(int i=0;i<colA;i++)
    {
        int index = arr[i].index;
        double x_tmp = x[index];
        sortx[i] = x_tmp;
        sortnnz_tmp[i] = arr[i].sortnnzpercol;
        index_tmp[i] = arr[i].index;
    }

  
    int tmp = 0;
    for(int i = 0; i< colA;i++)
    {
        int index = index_tmp[i];
    
        int start_tmp = csccolptr[index]; //
        int stop_tmp = csccolptr[index+1];
        for (int j = start_tmp;j<stop_tmp;j++)
        {
            int rowidx_tmp = cscrowidx[j];
            sortrowidx_tmp[tmp] = rowidx_tmp;
            double val_tmp = cscval[j];
            sortval_tmp[tmp]= val_tmp;
            tmp++;
        }
    }
}


void formattransation(slide_matrix *matrix,
                      int *sortrowidx_tmp,
                      double *sortval_tmp,
                      int *sortnnz_tmp,
                      int nnz,
                      int rowA,
                      int colA,
                      int h_count)

{  
    int *nnzcol_reduce = (int *)malloc(sizeof(int)*colA);
    int *nnzcol_ptr = (int *)malloc(sizeof(int)*(colA+1));



    for(int i = 0; i<colA;i++)
    {
        nnzcol_reduce[i]= sortnnz_tmp[i];
        nnzcol_ptr[i]= sortnnz_tmp[i];
    }

    exclusive_reduce(nnzcol_reduce,colA);
    exclusive_scan(nnzcol_ptr, colA+1);


    int tmptest1=0;
    for(int i = 0; i<colA;i++)
    {
        if(nnzcol_reduce[i] != 0)
        {tmptest1++;}
    }

    int *fortilecol = (int *)malloc(sizeof(int)*tmptest1);
    int *fortilennz = (int *)malloc(sizeof(int)*tmptest1);
    int *fortiletall = (int *)malloc(sizeof(int)*tmptest1);

    int tmp1 =0;

    if(nnzcol_reduce[0]!=0)
    {
        fortilecol[0]=0;
        fortilennz[0]=0;
        fortiletall[0]=nnzcol_reduce[0];
        tmp1=tmp1+1;
    }
    for(int i = 1; i<colA;i++)
    {
        if(nnzcol_reduce[i] != 0)
        {
            fortilecol[tmp1]=i;
            fortilennz[tmp1] = sortnnz_tmp[i-1];
            fortiletall[tmp1] = nnzcol_reduce[i];
            tmp1++;
        }
    }
    
   

    int colcount =0;
    int nnzcount =0;
    int tallcount = 0;
    int reducecount = 0;
    int countcolptr = 0;


    int tilenum = 0;
    for(int i = 0;i<tmp1;i++)
    {
        int j = fortilecol[i];
        int t = fortiletall[i];
        int tile = (colA-j)%slidesize ==0 ? (colA-j)/slidesize: (colA-j)/slidesize +1;
        int tilesum = tile * t;
        tilenum = tilenum+tilesum;
    }

    matrix->tilenum = tilenum; 
   
    matrix->tile_ptr = (int *)malloc(( 1 + matrix->tilenum)*sizeof(int));
    memset(matrix->tile_ptr,0,(1+matrix->tilenum)*sizeof(int));

    matrix->tile_len = (int *)malloc((matrix->tilenum)*sizeof(int));
    memset(matrix->tile_len,0,(matrix->tilenum)*sizeof(int));

    matrix->tile_colidx = (int *)malloc((matrix->tilenum)*sizeof(int));
    memset(matrix->tile_colidx,0,(matrix->tilenum)*sizeof(int));

    matrix->tile_tall = (int *)malloc((matrix->tilenum)*sizeof(int));
    memset(matrix->tile_tall,0,(matrix->tilenum)*sizeof(int));

    int *tile_ptr = matrix->tile_ptr;
    int *tile_len = matrix->tile_len;
    int *tile_colidx = matrix->tile_colidx;
    int *tile_tall = matrix->tile_tall;

    int *tile_tall_pre = (int *)malloc((matrix->tilenum)*sizeof(int));
    int *nnzpertilesort = (int *)malloc((matrix->tilenum)*sizeof(int));

    int tmp2 = 0;
    for(int i = 0;i<tmp1;i++)
    {
        int ii = fortilecol[i];
        int tile = (colA-ii)%slidesize ==0 ? (colA-ii)/slidesize: (colA-ii)/slidesize +1;
        int ti = fortiletall[i];
        for(int j = 0; j<tile;j++)
        {
            int len = j == tile-1? colA-ii - (tile-1)*slidesize : slidesize;
            int tall = nnzcol_reduce[ii];
            int colidx_tmp = ii + slidesize*j;
            for(int jx = 0; jx<ti;jx++)
            {
                tile_ptr[tmp2]= len;
                nnzpertilesort[tmp2] = len;
                tile_len[tmp2] = len;
                tile_colidx[tmp2]= colidx_tmp;
                tile_tall_pre[tmp2]=fortilennz[i]+jx;
                tmp2++;
            }
        }
    }


    
    if (tmp2 != matrix->tilenum)
    {
        printf("error tile\n");
    }
 

    exclusive_scan(tile_ptr,tmp2+1);



    if(tile_ptr[tmp2] != nnz)
    {
        printf("error\n");
    }
    else
    {
       //printf("Verify_2 PASS\n");
    }

    matrix->sortval = (double *)malloc(nnz * sizeof(double));
    memset(matrix->sortval,0,nnz*sizeof(double));
    matrix->sortrowidx = (int *)malloc(nnz * sizeof(int));
    memset(matrix->sortrowidx,0,nnz * sizeof(int));

    double *sortval = matrix->sortval;
    int *sortrowidx = matrix->sortrowidx;

    for(int i =0; i<tmp2;i++)
    {
        int colidx_start = tile_colidx[i];
        int len_tmp = tile_len[i];
        int colidx_stop = colidx_start + len_tmp;
        int tall_start = tile_tall_pre[i];
        for(int ri = colidx_start ; ri<colidx_stop;ri++)
        {
            int j = tile_ptr[i];
            int rii = nnzcol_ptr[ri] + tall_start;
            sortval[j] =sortval_tmp[rii];
            sortrowidx[j] = sortrowidx_tmp[rii];
            tile_ptr[i]++;   
        }
        
    }

    tile_ptr[0]=0;
    for(int i = tmp2-1;i>=1;i--)
    {
        tile_ptr[i]= tile_ptr[i]- nnzpertilesort[i];
    }



    free(nnzcol_reduce);
    free(nnzcol_ptr);
    free(fortilecol);
    free(fortilennz);
    free(tile_tall_pre);
    free(nnzpertilesort);
}

void tile_classification(slide_matrix *matrix,
                         int nnz,int rowA,int colA)
{  
    int *rowidx_tmpc = matrix->sortrowidx;
    int *tile_len_tmpc = matrix->tile_len;
    int *tile_ptr_tmpc = matrix->tile_ptr;
    int tilenumc = matrix->tilenum;
    

    matrix->tile_format = (int *)malloc(tilenumc * sizeof(int));
    memset(matrix->tile_format,0,tilenumc*sizeof(int));
    int *tile_formatc = matrix->tile_format;

    int samerow=0;
    int samerow2=0;
    int allsamerow=0;
    for(int i =0;i<tilenumc;i++)
    {
        int flag = 0;
        int length_tmp = tile_len_tmpc[i];
        int ptr_tmp = tile_ptr_tmpc[i];
        for(int j=ptr_tmp;j<ptr_tmp+length_tmp;j++)
        {
            int length1 = rowidx_tmpc[ptr_tmp];
            if(rowidx_tmpc[j] != length1)
            {flag = 1;break;}
        }
        if(flag == 0 && length_tmp == 32 )
        {
            samerow++;
            tile_formatc[i]=1;
        }
 
        if(flag ==0)
        {
            samerow2++;
        }
        if(flag ==0 && length_tmp ==32)
        {
            allsamerow++;
        }
    }
    //printf("Count of all rowidx from same row in a tile: all: %d, regular same row : %d, except size 1: %d \n", samerow2, allsamerow, samerow);

    matrix->countrow = (int *)malloc((tilenumc+1) * sizeof(int));
    memset(matrix->countrow,0,(tilenumc +1) * sizeof(int));
    int *countrow = matrix->countrow;

    for(int i = 0;i < tilenumc;i++)
    {
        countrow[i] = 1;
        int len = tile_len_tmpc[i];
        int start = tile_ptr_tmpc[i];
        int stop = start + len;
        int compare = rowidx_tmpc[start];
        for(int j = start+1; j<stop; j++)
        {
            if(rowidx_tmpc[j] != compare)
            {
                compare = rowidx_tmpc[j];
                countrow[i]++;
            }
        }
    }
    exclusive_scan(countrow,(tilenumc + 1));

    int segsum = countrow[tilenumc];
    matrix->segsum = segsum;

    matrix->segmentoffset = (int *)malloc((segsum+1) * sizeof(int));
    memset(matrix->segmentoffset,0,(segsum +1) * sizeof(int));
    int *segmentoffset = matrix->segmentoffset;

    int *segmentoffset_tmp = (int *)malloc(sizeof(int)*(segsum+1));
    memset(segmentoffset_tmp,0,(segsum +1) * sizeof(int));


    for(int i=0;i<tilenumc;i++)
    {
        int len = tile_len_tmpc[i];
        int start = tile_ptr_tmpc[i];
        int stop = start + len;
        int segpoint = countrow[i];
        int compare = rowidx_tmpc[start];
        for(int j = start+1;j<stop;j++)
        {
            if(rowidx_tmpc[j] == compare)
            {
                segmentoffset[segpoint]++;
                segmentoffset_tmp[segpoint]++;
            }
            else
            {
                compare = rowidx_tmpc[j];
                segpoint++;
            }

        }
    }
    for(int i = 0; i<segsum;i++)
    {
        segmentoffset[i] +=1;
        segmentoffset_tmp[i] +=1; 
    }

    exclusive_scan(segmentoffset_tmp,(segsum+1));

    if(segmentoffset_tmp[segsum] != nnz)
    {
        printf("error segmentoffset: error is : %d \n", segmentoffset_tmp[segsum]);
    }


    matrix->sortrowindex = (int *)malloc((nnz) * sizeof(int));
    memset(matrix->sortrowindex,0,(nnz) * sizeof(int));
    int *sortrowindex = matrix->sortrowindex;

    for(int i =0; i<tilenumc;i++)
    {
        int lent = tile_len_tmpc[i];
        int start = countrow[i];
        int stop = countrow[i+1];
        for(int j = start; j<stop; j++)
        {
            int index = segmentoffset_tmp[j];
            int targetval = segmentoffset[j];
            sortrowindex[index] = targetval;
        }
        if(stop - start !=1 && lent == 32)
        {
            tile_formatc[i]=2;
        }
    }

}
