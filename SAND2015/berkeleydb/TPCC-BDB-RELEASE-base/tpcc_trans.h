/* Input and output data for transactions 
 * These definitions are taken from tpcc_client.h. Any changes made
 * in tpcc_client.h must be reflected here.
 */
#include <time.h>

typedef enum transaction_type
{
    MIXED,
    PAYMENT,
    NEWORDER,
    ORDERSTATUS,
    DELIVERY,
    STOCKLEVEL
} txn_type_t;

/////////////////////////////////////////////////////////////////////////
typedef struct neworder_item_info
{
    int    ol_supply_w_id;
    int    ol_i_id;
    char   i_name[25];
    int    ol_quantity;
    int    s_quantity;
    char   brand[2];
    double i_price;
    double ol_amount;
}NEWORDER_ITEM_INFO;

typedef struct neworder_transaction_data
{
    int    w_id;
    int    d_id;
    int    c_id;
    int    o_id;
    int    o_ol_cnt;
    int    padding;
    double c_discount;
    double w_tax;
    double d_tax;
    char   o_entry_d[20];
    char   c_credit[3];
    char   c_last[17];
    NEWORDER_ITEM_INFO order_data[15];
    char   status[25];
    int    padding2;
    double total;
}NEWORDER_TRANSACTION_DATA;

/////////////////////////////////////////////////////////////////////////

typedef struct payment_transaction_data
{
    int    w_id;
    int    d_id;
    int    c_id;       /* If -1, then use c_last */
    int    c_w_id;
    int    c_d_id;
    int    padding1;
    double h_amount;
    double c_credit_lim;
    double c_balance;
    double c_discount;
    char   h_date[20];
    char   w_street_1[21];
    char   w_street_2[21];
    char   w_city[21];
    char   w_state[3];
    char   w_zip[11];
    char   d_street_1[21];
    char   d_street_2[21];
    char   d_city[21];
    char   d_state[3];
    char   d_zip[11];
    char   c_first[17];
    char   c_middle[3];
    char   c_last[17];
    char   c_street_1[21];
    char   c_street_2[21];
    char   c_city[21];
    char   c_state[3];
    char   c_zip[11];
    char   c_phone[17];
    char   c_since[11];
    char   c_credit[3];
    char   c_data_1[51];
    char   c_data_2[51];
    char   c_data_3[51];
    char   c_data_4[51];
    int    padding2;
}PAYMENT_TRANSACTION_DATA;

/////////////////////////////////////////////////////////////////////////

typedef struct orderstatus_item_info {

    int    ol_supply_w_id;
    int    ol_i_id; 
    int	   ol_quantity;
    int    padding1;
    double ol_amount;
    char   ol_delivery_d[11];
    int    padding2;
}ORDERSTATUS_ITEM_INFO;

typedef struct orderstatus_transaction_data
{
    int    w_id;
    int    d_id;
    int    c_id;
    int    o_id;
    int    o_carrier_id;
    int    item_cnt;
    double c_balance;
    char   c_first[17];
    char   c_middle[3];
    char   c_last[17];
    char   o_entry_d[20];
    int    padding;
    ORDERSTATUS_ITEM_INFO order_data[15];  
}ORDERSTATUS_TRANSACTION_DATA;

/////////////////////////////////////////////////////////////////////////

typedef struct delivery_transaction_data
{
    int  w_id;
    int  o_carrier_id;
    char status[26];
}DELIVERY_TRANSACTION_DATA;

typedef struct delivery_queue_record
{
    int    w_id;
    int    o_carrier_id;
    time_t qtime;
}DELIVERY_QUEUE_RECORD;

/////////////////////////////////////////////////////////////////////////

typedef struct stocklevel_transaction_data
{
    int w_id;
    int d_id;
    int threshold;
    int low_stock;

}STOCKLEVEL_TRANSACTION_DATA;

/* Miscellaneous */
char        *get_txn_type_string(txn_type_t txn_type);
