
#define DRIVER_INTERFACE_VERSION 1

enum drvc
        {
        drv_interfaceversion=1, drv_ident, drv_version,
        drv_config, drv_confinfo, drv_init_device, drv_get_ch_cnt,
        drv_exit, drv_ch_active, drv_init_kanal,
        drv_stat, drv_ch_state, drv_scale,
        drv_tx_calib, drv_set_led, drv_rx_frame,
        drv_get_framebuf, drv_tx_frame,
        drv_get_txdelay, drv_get_mode, drv_get_baud,
        drv_set_txdelay
        };

