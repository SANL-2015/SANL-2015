
/*
 * Definition of TPC-C schema. 
 * All character arrays are 1 char longer than required to 
 * account for the null character at the end of the string.
 */

////////////////////////////////////////////////////////////////////////////

typedef struct warehouse_primary_key
{
    int W_ID;
}WAREHOUSE_PRIMARY_KEY;

typedef struct warehouse_primary_data
{
    char   W_NAME[11];
    char   W_STREET_1[21];
    char   W_STREET_2[21];
    char   W_CITY[21];
    char   W_STATE[3];
    char   W_ZIP[10];
    double W_TAX;
    double W_YTD;
}WAREHOUSE_PRIMARY_DATA;


////////////////////////////////////////////////////////////////////////////

typedef struct district_primary_key
{
    int D_W_ID;
    int D_ID;

}DISTRICT_PRIMARY_KEY;

typedef struct district_primary_data
{
    char   D_NAME[11];
    char   D_STREET_1[21];
    char   D_STREET_2[21];
    char   D_CITY[21];
    char   D_STATE[3];
    char   D_ZIP[10];
    double D_TAX;
    long   D_YTD;
    int    D_NEXT_O_ID;

}DISTRICT_PRIMARY_DATA;

////////////////////////////////////////////////////////////////////////////

typedef struct customer_primary_key
{
    int C_W_ID;
    int C_D_ID;
    int C_ID;

}CUSTOMER_PRIMARY_KEY;

typedef struct customer_primary_data
{
    char   C_FIRST[17];
    char   C_MIDDLE[3];
    char   C_LAST[17];
    char   C_STREET_1[21];
    char   C_STREET_2[21];
    char   C_CITY[21];
    char   C_STATE[3];
    char   C_ZIP[10];
    char   C_PHONE[16];
    char   C_SINCE[26];
    char   C_CREDIT[3];
    double C_CREDIT_LIM;
    double C_DISCOUNT;
    double C_BALANCE;
    long   C_YTD_PAYMENT;
    int    C_PAYMENT_CNT;
    int    C_DELIVERY_CNT;
    char   C_DATA[501];

}CUSTOMER_PRIMARY_DATA;

/* Secondary index to allow lookup by last name */
typedef struct customer_secondary_key
{
    int  C_W_ID;
    int  C_D_ID;
    char C_LAST[17];
    char C_FIRST[17];
    int  C_ID;
}CUSTOMER_SECONDARY_KEY;

////////////////////////////////////////////////////////////////////////////

typedef struct history_primary_key
{
    int     H_C_ID;
    int     H_C_D_ID;
    int     H_C_W_ID;
    int     H_D_ID;
    int     H_W_ID;
    char    H_DATE[26];
    double  H_AMOUNT;
    char    H_DATA[25];

}HISTORY_PRIMARY_KEY;

////////////////////////////////////////////////////////////////////////////

typedef struct neworder_primary_key
{
    int NO_W_ID;
    int NO_D_ID;
    int NO_O_ID;

}NEWORDER_PRIMARY_KEY;

////////////////////////////////////////////////////////////////////////////

typedef struct order_primary_key
{
    int O_W_ID;
    int O_D_ID;
    int O_ID;

}ORDER_PRIMARY_KEY;

typedef struct order_primary_data
{
    int  O_C_ID;
    char O_ENTRY_D[26];
    int  O_CARRIER_ID;
    char O_OL_CNT;
    char O_ALL_LOCAL;

}ORDER_PRIMARY_DATA;

/* Secondary index to allow look-ups by O_W_ID, O_D_ID, O_C_ID */
typedef struct order_secondary_key
{
    int O_W_ID;
    int O_D_ID;
    int O_C_ID;

}ORDER_SECONDARY_KEY;

////////////////////////////////////////////////////////////////////////////

typedef struct orderline_primary_key
{
    int OL_W_ID;
    int OL_D_ID;
    int OL_O_ID;
    int OL_NUMBER;

}ORDERLINE_PRIMARY_KEY;

typedef struct orderline_primary_data
{
    int    OL_I_ID;
    int    OL_SUPPLY_W_ID;
    char   OL_DELIVERY_D[26];
    char   OL_QUANTITY;
    double OL_AMOUNT;
    char   OL_DIST_INFO[25];

}ORDERLINE_PRIMARY_DATA;

////////////////////////////////////////////////////////////////////////////

typedef struct item_primary_key
{
    int I_ID;

}ITEM_PRIMARY_KEY;

typedef struct item_primary_data
{
    int  I_IM_ID;
    char I_NAME[25];
    int  I_PRICE;
    char I_DATA[51];

}ITEM_PRIMARY_DATA;

////////////////////////////////////////////////////////////////////////////

typedef struct stock_primary_key
{
    int S_W_ID;
    int S_I_ID;

}STOCK_PRIMARY_KEY;

typedef struct stock_primary_data
{
    int  S_QUANTITY;
    char S_DIST_01[25];
    char S_DIST_02[25];
    char S_DIST_03[25];
    char S_DIST_04[25];
    char S_DIST_05[25];
    char S_DIST_06[25];
    char S_DIST_07[25];
    char S_DIST_08[25];
    char S_DIST_09[25];
    char S_DIST_10[25];
    long S_YTD;
    int  S_ORDER_CNT;
    int  S_REMOTE_CNT;
    char S_DATA[51];

}STOCK_PRIMARY_DATA;

////////////////////////////////////////////////////////////////////////////
