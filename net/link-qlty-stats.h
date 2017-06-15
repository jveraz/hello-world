/*
 * Copyright (c) 2017, VLC Spain ITI.
 * All rights reserved.
 *
 * 
 *
 * Authors: CAINE group
 */

#include "core/net/linkaddr.h"
#include "net/mac/tsch/tsch-conf.h"

/* All statistics of a given link */
struct link_qlty_stats {
  int16_t rssi[sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)];		/* RSSI (received signal strength) */
  uint8_t channel;	      					/* Last channel of reception */
  clock_time_t last_rx_time;					/* Last Rx timestamp */
};

/* Returns the neighbor's link statistics */
const struct link_qlty_stats *link_qlty_stats_from_lladdr(const linkaddr_t *lladdr);

/* Funcion para actualizar los valores */
void link_qlty_stats_updater(const linkaddr_t *lladdr, int16_t l_rssi, uint8_t ch);

/* Funcion para calcular la media de los valores RSSI */
int16_t rssi_mean(int16_t recv_ssi[sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)]);

/* Funciones de mapeo */
uint16_t rssi_mapping(int16_t recv_ssi[sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)]);
uint16_t etx_mapping(uint16_t recv_etx);
uint16_t nbr_mapping(int nbr_num);
uint16_t hop_mapping(uint16_t hopcount);

/* Initializes link-qlty-stats module */
void link_qlty_stats_init(void);


