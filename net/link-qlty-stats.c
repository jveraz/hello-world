/*
 * Copyright (c) 2017, VLC Spain ITI.
 * All rights reserved.
 *
 * 
 *
 * Authors: CAINE group
 */

#include "contiki.h"
#include "sys/clock.h"
#include "net/link-qlty-stats.h"
#include "net/nbr-table.h"
#include "net/rpl/rpl.h"
#include <stdio.h>

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* Per-neighbor link quality statistics table */
NBR_TABLE(struct link_qlty_stats, link_qlty_stats);

/* Called every VARIABLE minutes to update RSSI values */
struct ctimer periodic_timer;

/* RSSI statistics with no update in RSSI_FRESHNESS_TIME is not fresh */
#define RSSI_FRESHNESS_TIME		(2 * 60 * (clock_time_t)CLOCK_SECOND)

/* EWMA (exponential moving average) used to maintain statistics over time */
#define EWMA_SCALE            100
#define EWMA_ALPHA             15

/* Valores para definir las funciones de mapeo */
#define MIN_MAP		      128
#define MAX_MAP		      512

#define ETX_MIN		      128
#define ETX_MAX 	      512

#define RSSI_MIN	      -82
#define RSSI_MAX	      -62

#define NBR_MIN		        1
#define NBR_MAX			8

#define HOP_MIN			1
#define HOP_MAX			8

/*---------------------------------------------------------------------------*/
/* Returns the neighbor's link quality stats */
const struct link_qlty_stats *
link_qlty_stats_from_lladdr(const linkaddr_t *lladdr)
{
  return nbr_table_get_from_lladdr(link_qlty_stats, lladdr);
}
/*---------------------------------------------------------------------------*/
/* Funcion para actualizar los valores */
void link_qlty_stats_updater(const linkaddr_t *lladdr, int16_t l_rssi, uint8_t ch)
{
  struct link_qlty_stats *qlty_stats;
  int i;
  qlty_stats = nbr_table_get_from_lladdr(link_qlty_stats, lladdr);
  if(qlty_stats == NULL){
    // Añadimos el vecino
    qlty_stats = nbr_table_add_lladdr(link_qlty_stats, lladdr, NBR_TABLE_REASON_LINK_STATS, NULL); /* Aqui no entiendo para que es el REASON */
    if(qlty_stats != NULL){
      // Inicializamos 
      qlty_stats->last_rx_time = clock_time();
      for(i = 0; i < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE); i++){
	// Iniciamos todos los valores con el mismo RSSI
        qlty_stats->rssi[i] = l_rssi;
      }
      qlty_stats->channel = ch;
    }
    return;
  }
      
  // Actualizamos los valores
  for(i = 0; i < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE); i++){
    if(ch == TSCH_DEFAULT_HOPPING_SEQUENCE[i]){
      /*qlty_stats->rssi[i] = l_rssi;*/
      qlty_stats->rssi[i] = ((int32_t)qlty_stats->rssi[i] * (EWMA_SCALE - EWMA_ALPHA) + (int32_t)l_rssi * EWMA_ALPHA) / EWMA_SCALE;
    }
  }
  qlty_stats->last_rx_time = clock_time();
  qlty_stats->channel = ch;
}
/*---------------------------------------------------------------------------*/
static void 
periodic(void *ptr)
{
  struct link_qlty_stats *qlty_stats;
  ctimer_reset(&periodic_timer);
  int i;

  for(qlty_stats = nbr_table_head(link_qlty_stats); qlty_stats != NULL; qlty_stats = nbr_table_next(link_qlty_stats, qlty_stats)) {
    if(clock_time() - qlty_stats->last_rx_time > RSSI_FRESHNESS_TIME) {
      PRINTF("Tiempo expirado\n");
      for(i = 0; i < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE); i++){
        qlty_stats->rssi[i] = -90;
        /*qlty_stats->rssi[i] = ((int32_t)qlty_stats->rssi[i] * (EWMA_SCALE - EWMA_ALPHA) + (int32_t)l_rssi * EWMA_ALPHA) / EWMA_SCALE;*/
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Initializes link-qlty-stats module */
void 
link_qlty_stats_init(void)
{
  nbr_table_register(link_qlty_stats, NULL);
  ctimer_set(&periodic_timer, 30 * (clock_time_t)CLOCK_SECOND, periodic, NULL);
}
/*---------------------------------------------------------------------------*/
int16_t
rssi_mean(int16_t recv_ssi[sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)])
{
  int32_t mean = 0;
  int i;
  for(i = 0; i < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE); i++){
    mean = mean + (int32_t)recv_ssi[i];
  }
  return mean / sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE);
  /*float mean = 0.0;
  int i;
  for(i = 0; i < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE); i++){
    mean = mean + 1.0/(float)recv_ssi[i];
  }
  return (int16_t)((float)sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE) / mean);*/
}
/*---------------------------------------------------------------------------*/
uint16_t
rssi_mapping(int16_t recv_ssi[sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)])
{
  int16_t signal_mean = rssi_mean(recv_ssi);
  
  if((signed)signal_mean < RSSI_MIN) {
    return (uint16_t)MAX_MAP;
  } else {
    if((signed)signal_mean < RSSI_MAX) {
      return (uint16_t)(MAX_MAP-((float)(MAX_MAP-MIN_MAP)/(float)(RSSI_MAX-RSSI_MIN))*((signed)signal_mean-RSSI_MIN));
    } else {
      return (uint16_t)MIN_MAP;
    }
  }
}
/*---------------------------------------------------------------------------*/
uint16_t
etx_mapping(uint16_t recv_etx)
{
  if(recv_etx < ETX_MIN) {
    return (uint16_t)MIN_MAP;
  } else {
    if(recv_etx < ETX_MAX) {
      return recv_etx; /* ETX ya viene multiplicado por la escala *128 */
    } else {
      return (uint16_t)MAX_MAP;
    }
  }
}
/*---------------------------------------------------------------------------*/
uint16_t
nbr_mapping(int nbr_num)
{
  if(nbr_num <= NBR_MIN) {
    return (uint16_t)MAX_MAP;
  } else {
    if(nbr_num <= NBR_MAX) {
      return (uint16_t)(MAX_MAP-((MAX_MAP-MIN_MAP)/(float)(NBR_MAX-NBR_MIN))*(nbr_num-NBR_MIN)); /* Mapeo */
    } else {
      return (uint16_t)MIN_MAP;
    }
  }
}
/*---------------------------------------------------------------------------*/
uint16_t
hop_mapping(uint16_t hopcount)
{
  hopcount = hopcount/128;
  if(hopcount <= HOP_MIN) {
    return (uint16_t)MIN_MAP;
  } else {
    if(hopcount < HOP_MAX) {
      return (uint16_t)(MIN_MAP+((MAX_MAP-MIN_MAP)/(HOP_MAX-HOP_MIN))*(hopcount-HOP_MIN)); 
    } else {
      return (uint16_t)MAX_MAP;
    }
  }
}
