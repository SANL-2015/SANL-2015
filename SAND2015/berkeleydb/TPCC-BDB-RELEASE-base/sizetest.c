#include "tpcc_trans.h"

int
main(void)
{
  PAYMENT_TRANSACTION_DATA ps;
  ORDERSTATUS_TRANSACTION_DATA os;
  ORDERSTATUS_ITEM_INFO ois;
  NEWORDER_ITEM_INFO nis;
  NEWORDER_TRANSACTION_DATA no;

  char bytes[8];
  int i;
	
  printf("Size of pay_struct is %d\n", sizeof(PAYMENT_TRANSACTION_DATA));
  printf("Size of int is %d\n", sizeof(int));
  printf("Size of double is %d\n", sizeof(double));	

  printf("Alignment of int is %d\n", __alignof__(int));
  printf("Alignment of double is %d\n", __alignof__(double));	
  printf("Alignment of char is %d\n", __alignof__(char));	
  
  printf("Address of ps = %d\n", &ps);
  printf("Address of ps.w_id          = %x\n", &ps.w_id);
  printf("Address of ps.d_id          = %x\n", &ps.d_id);
  printf("Address of ps.c_id          = %x\n", &ps.c_id);
  printf("Address of ps.c_w_id        = %x\n", &ps.c_w_id);
  printf("Address of ps.c_d_id        = %x\n", &ps.c_d_id);
  printf("Address of ps.h_amount      = %x\n",  &ps.h_amount);
  printf("Address of ps.c_credit_lim  = %x\n",  &ps.c_credit_lim);
  printf("Address of ps.c_balance     = %x\n",  &ps.c_balance);
  printf("Address of ps.c_discount    = %x\n",  &ps.c_discount);
  printf("Address of ps.h_date        = %x\n",  ps.h_date);

  ps.h_amount = 1;
  printf("ps.h_amount is %f\n", ps.h_amount);
  memcpy(bytes, &ps.h_amount, 8);
  for(i = 0; i<8; i++)
  {
	printf("Byte %d = %d\n", i, bytes[i]);
  }	

  printf("Size of ord_struct is %d\n", sizeof(ORDERSTATUS_TRANSACTION_DATA));
  printf("Size of ord_itm_struct is %d\n", sizeof(ORDERSTATUS_ITEM_INFO));

  printf("Address of os = %x\n", &os);
  printf("Address of os.w_id          = %x\n", &os.w_id);
  printf("Address of os.d_id          = %x\n", &os.d_id);
  printf("Address of os.c_id          = %x\n", &os.c_id);
  printf("Address of os.o_id          = %x\n", &os.o_id);
  printf("Address of os.o_carrier_id  = %x\n", &os.o_carrier_id);
  printf("Address of os.item_cnt      = %x\n", &os.item_cnt);
  printf("Address of os.c_balance     = %x\n", &os.c_balance);
  printf("Address of os.c_first       = %x\n", os.c_first);

  for(i = 0; i<15; i++)
  {
      printf("Address of os.order_data %d = %x\n", i, &os.order_data[i]);
  }

   printf("\n\nAddress of ois.ol_supply_w_id  = %x\n", &ois.ol_supply_w_id);
   printf("Address of ois.ol_i_id  	 = %x\n", &ois.ol_i_id);
   printf("Address of ois.ol_i_id        = %x\n", &ois.ol_i_id);
   printf("Address of ois.ol_quantity    = %x\n", &ois.ol_quantity);
   printf("Address of ois.ol_amount      = %x\n", &ois.ol_amount);
   printf("Address of ois.ol_delivery_d  = %x\n", ois.ol_delivery_d);

   printf("\n\nSize of no_itm_struct is %d\n", sizeof(NEWORDER_ITEM_INFO));
   printf("Address of nis  	 = %x\n", &nis);
   printf("Address of nis.ol_supply_w_id = %x\n", &nis.ol_supply_w_id);
   printf("Address of nis.ol_i_id        = %x\n", &nis.ol_i_id);
   printf("Address of nis.i_name         = %x\n", nis.i_name);
   printf("Address of nis.ol_quantity    = %x\n", &nis.ol_quantity);
   printf("Address of nis.s_quantity     = %x\n", &nis.s_quantity);
   printf("Address of nis.brand          = %x\n", nis.brand);
   printf("Address of nis.i_price        = %x\n", &nis.i_price);
   printf("Address of nis.ol_amount      = %x\n", &nis.ol_amount);

   printf("\n\nSize of no_struct is %d\n", sizeof(NEWORDER_TRANSACTION_DATA));
   printf("Address of no  	         = %x\n", &no);
   printf("Address of no.w_id            = %x\n", &no.w_id);
   printf("Address of no.d_id            = %x\n", &no.d_id);
   printf("Address of no.c_id            = %x\n", &no.c_id);
   printf("Address of no.o_id            = %x\n", &no.o_id);
   printf("Address of no.o_ol_cnt        = %x\n", &no.o_ol_cnt);
   printf("Address of no.c_discount      = %x\n", &no.c_discount);
   printf("Address of no.w_tax           = %x\n", &no.w_tax);
   printf("Address of no.d_tax           = %x\n", &no.d_tax);
   printf("Address of no.o_entry_d       = %x\n", no.o_entry_d);	
   printf("Address of no.c_credit        = %x\n", no.c_credit);
   printf("Address of no.c_last          = %x\n", no.c_last);		
   printf("Address of no.n_items[0]      = %x\n", &no.order_data[0]);	
   printf("Address of no.n_items[14]     = %x\n", &no.order_data[14]);	
   printf("Address of no.status          = %x\n", no.status);
   printf("Address of no.total           = %x\n", &no.total);

}
