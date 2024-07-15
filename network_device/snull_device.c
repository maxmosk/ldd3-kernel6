#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include "snull_device.h"

static int lockup = LOCKUP;
static int timeout = SNULL_TIMEOUT;
static int use_napi = USE_NAPI;
static int pool_size = POOL_SIZE;
struct net_device *snull_devs[2];


struct snull_packet {
    struct snull_packet *next;
    struct net_device *dev;
    int	datalen;
    u8 data[ETH_DATA_LEN];
};


struct snull_priv {
    struct net_device_stats stats;
    int status;
    struct snull_packet *ppool;
    struct snull_packet *rx_queue;  /* List of incoming packets */
    int rx_int_enabled;
    int tx_packetlen;
    u8 *tx_packetdata;
    struct sk_buff *skb;
    spinlock_t lock;
    struct net_device *dev;
    struct napi_struct napi;
};


static void (*snull_interrupt)(int, void *, struct pt_regs *);


void snull_setup_pool(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    int i;
    struct snull_packet *pkt;

    priv->ppool = NULL;
    for (i = 0; i < pool_size; i++) {
        pkt = kmalloc (sizeof (struct snull_packet), GFP_KERNEL);
        if (pkt == NULL) {
            pr_notice("SNULL: Ran out of memory allocating packet pool\n");
            return;
        }
        pkt->dev = dev;
        pkt->next = priv->ppool;
        priv->ppool = pkt;
    }
}


void snull_teardown_pool(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    struct snull_packet *pkt;
    
    while ((pkt = priv->ppool)) {
        priv->ppool = pkt->next;
        kfree (pkt);
    }
}


struct snull_packet *snull_get_tx_buffer(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    unsigned long flags;
    struct snull_packet *pkt;
    
    spin_lock_irqsave(&priv->lock, flags);
    pkt = priv->ppool;
    if(!pkt) {
        pr_info("SNULL: Out of Pool\n");
        return pkt;
    }
    priv->ppool = pkt->next;
    if (priv->ppool == NULL) {
        pr_info("SNULL: Pool empty\n");
        netif_stop_queue(dev);
    }
    spin_unlock_irqrestore(&priv->lock, flags);
    return pkt;
}


void snull_release_buffer(struct snull_packet *pkt)
{
    unsigned long flags;
    struct snull_priv *priv = netdev_priv(pkt->dev);
    
    spin_lock_irqsave(&priv->lock, flags);
    pkt->next = priv->ppool;
    priv->ppool = pkt;
    spin_unlock_irqrestore(&priv->lock, flags);
    if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
        netif_wake_queue(pkt->dev);
}


void snull_enqueue_buf(struct net_device *dev, struct snull_packet *pkt)
{
    unsigned long flags;
    struct snull_priv *priv = netdev_priv(dev);

    spin_lock_irqsave(&priv->lock, flags);
    pkt->next = priv->rx_queue;  /* FIXME - misorders packets */
    priv->rx_queue = pkt;
    spin_unlock_irqrestore(&priv->lock, flags);
}


struct snull_packet *snull_dequeue_buf(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    struct snull_packet *pkt;
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    pkt = priv->rx_queue;
    if (pkt != NULL)
        priv->rx_queue = pkt->next;
    spin_unlock_irqrestore(&priv->lock, flags);
    return pkt;
}


static void snull_rx_ints(struct net_device *dev, int enable)
{
    struct snull_priv *priv = netdev_priv(dev);
    priv->rx_int_enabled = enable;
}


int __snull_open(struct net_device *dev)
{
    pr_info("SNULL: open was called\n");
    dev_addr_set(dev, "\0SNUL0");
    if (dev == snull_devs[1]) {
        u8* new_addr = "\0SNUL1";
        dev_addr_set(dev, new_addr);
    }
    if (use_napi) {
        struct snull_priv *priv = netdev_priv(dev);
        napi_enable(&priv->napi);
    }

    netif_start_queue(dev);
    return 0;
}


int __snull_release(struct net_device *dev)
{
    netif_stop_queue(dev); /* can't transmit any more */
        if (use_napi) {
                struct snull_priv *priv = netdev_priv(dev);
                napi_disable(&priv->napi);
        }
    return 0;
}


int __snull_config(struct net_device *dev, struct ifmap *map)
{
    if (dev->flags & IFF_UP) /* can't act on a running interface */
        return -EBUSY;

    /* Don't allow changing the I/O address */
    if (map->base_addr != dev->base_addr) {
        pr_warn("SNULL: Can't change I/O address\n");
        return -EOPNOTSUPP;
    }

    /* Allow changing the IRQ */
    if (map->irq != dev->irq) {
        dev->irq = map->irq;
            /* request_irq() is delayed to open-time */
    }

    /* ignore other fields */
    return 0;
}


void snull_rx(struct net_device *dev, struct snull_packet *pkt)
{
    struct sk_buff *skb;
    struct snull_priv *priv = netdev_priv(dev);

    /*
     * The packet has been retrieved from the transmission
     * medium. Build an skb around it, so upper layers can handle it
     */
    skb = dev_alloc_skb(pkt->datalen + 2);
    if (!skb) {
        if (printk_ratelimit())
            pr_notice("SNULL: rx low on mem - packet dropped\n");
        priv->stats.rx_dropped++;
        goto out;
    }
    skb_reserve(skb, 2); /* align IP on 16B boundary */  
    memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

    /* Write metadata, and then pass to the receive level */
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += pkt->datalen;
    netif_rx(skb);
  out:
    return;
}
    

static int snull_poll(struct napi_struct *napi, int budget)
{
    int npackets = 0;
    struct sk_buff *skb;
    struct snull_priv *priv = container_of(napi, struct snull_priv, napi);
    struct net_device *dev = priv->dev;
    struct snull_packet *pkt;
    
    while (npackets < budget && priv->rx_queue) {
        pkt = snull_dequeue_buf(dev);
        skb = dev_alloc_skb(pkt->datalen + 2);
        if (! skb) {
            if (printk_ratelimit())
                pr_notice("SNULL: packet dropped\n");
            priv->stats.rx_dropped++;
            npackets++;
            snull_release_buffer(pkt);
            continue;
        }
        skb_reserve(skb, 2); /* align IP on 16B boundary */  
        memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
        skb->dev = dev;
        skb->protocol = eth_type_trans(skb, dev);
        skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
        netif_receive_skb(skb);
        
            /* Maintain stats */
        npackets++;
        priv->stats.rx_packets++;
        priv->stats.rx_bytes += pkt->datalen;
        snull_release_buffer(pkt);
    }
    /* If we processed all packets, we're done; tell the kernel and reenable ints */
    //if (! priv->rx_queue) {
    if (npackets < budget) {
        unsigned long flags;
        spin_lock_irqsave(&priv->lock, flags);
        if (napi_complete_done(napi, npackets))
            snull_rx_ints(dev, 1);
        spin_unlock_irqrestore(&priv->lock, flags);
        //return 0; // fall in return packets
    }
    /* We couldn't process everything. */
    return npackets;
}
        

static void snull_regular_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int statusword;
    struct snull_priv *priv;
    struct snull_packet *pkt = NULL;
    /*
     * As usual, check the "device" pointer to be sure it is
     * really interrupting.
     * Then assign "struct device *dev"
     */
    struct net_device *dev = (struct net_device *)dev_id;
    /* ... and check with hw if it's really ours */

    /* paranoid */
    if (!dev)
        return;

    /* Lock the device */
    priv = netdev_priv(dev);
    spin_lock(&priv->lock);

    /* retrieve statusword: real netdevices use I/O instructions */
    statusword = priv->status;
    priv->status = 0;
    if (statusword & SNULL_RX_INTR) {
        /* send it to snull_rx for handling */
        pkt = priv->rx_queue;
        if (pkt) {
            priv->rx_queue = pkt->next;
            snull_rx(dev, pkt);
        }
    }
    if (statusword & SNULL_TX_INTR) {
        /* a transmission is over: free the skb */
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += priv->tx_packetlen;
        dev_kfree_skb(priv->skb);
    }

    /* Unlock the device and we are done */
    spin_unlock(&priv->lock);
    if (pkt) snull_release_buffer(pkt); /* Do this outside the lock! */
    return;
}


static void snull_napi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int statusword;
    struct snull_priv *priv;

    /*
     * As usual, check the "device" pointer for shared handlers.
     * Then assign "struct device *dev"
     */
    struct net_device *dev = (struct net_device *)dev_id;
    /* ... and check with hw if it's really ours */

    /* paranoid */
    if (!dev)
        return;

    /* Lock the device */
    priv = netdev_priv(dev);
    spin_lock(&priv->lock);

    /* retrieve statusword: real netdevices use I/O instructions */
    statusword = priv->status;
    priv->status = 0;
    if (statusword & SNULL_RX_INTR) {
        snull_rx_ints(dev, 0);  /* Disable further interrupts */
        napi_schedule(&priv->napi);
    }
    if (statusword & SNULL_TX_INTR) {
            /* a transmission is over: free the skb */
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += priv->tx_packetlen;
        if(priv->skb) {
            dev_kfree_skb(priv->skb);
            priv->skb = 0;
        }
    }

    /* Unlock the device and we are done */
    spin_unlock(&priv->lock);
    return;
}


static void snull_hw_tx(char *buf, int len, struct net_device *dev)
{
    /*
     * This function deals with hw details. This interface loops
     * back the packet to the other snull interface (if any).
     * In other words, this function implements the snull behaviour,
     * while all other procedures are rather device-independent
     */
    struct iphdr *ih;
    struct net_device *dest;
    struct snull_priv *priv;
    u32 *saddr, *daddr;
    struct snull_packet *tx_buffer;
    
    /* I am paranoid. Ain't I? */
    if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
        pr_warn("SNULL: Hmm... packet too short (%i octets)\n",
                len);
        return;
    }

    if (0) { /* enable this conditional to look at the data */
        int i;
        pr_info("SNULL: len is %i\n" KERN_DEBUG "data:",len);
        for (i=14 ; i<len; i++)
            printk(" %02x",buf[i]&0xff);
        printk("\n");
    }
    /*
     * Ethhdr is 14 bytes, but the kernel arranges for iphdr
     * to be aligned (i.e., ethhdr is unaligned)
     */
    ih = (struct iphdr *)(buf+sizeof(struct ethhdr));
    saddr = &ih->saddr;
    daddr = &ih->daddr;

    ((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
    ((u8 *)daddr)[2] ^= 1;

    ih->check = 0;         /* and rebuild the checksum (ip needs it) */
    ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);

    if (dev == snull_devs[0])
        pr_info("%08x:%05i --> %08x:%05i\n",
                ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source),
                ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest));
    else
        pr_info("%08x:%05i <-- %08x:%05i\n",
                ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest),
                ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source));

    /*
     * Ok, now the packet is ready for transmission: first simulate a
     * receive interrupt on the twin device, then  a
     * transmission-done on the transmitting device
     */
    dest = snull_devs[dev == snull_devs[0] ? 1 : 0];
    priv = netdev_priv(dest);
    tx_buffer = snull_get_tx_buffer(dev);

    if(!tx_buffer) {
        pr_info("SNULL: Out of tx buffer, len is %i\n",len);
        return;
    }

    tx_buffer->datalen = len;
    memcpy(tx_buffer->data, buf, len);
    snull_enqueue_buf(dest, tx_buffer);
    if (priv->rx_int_enabled) {
        priv->status |= SNULL_RX_INTR;
        snull_interrupt(0, dest, NULL);
    }

    priv = netdev_priv(dev);
    priv->tx_packetlen = len;
    priv->tx_packetdata = buf;
    priv->status |= SNULL_TX_INTR;
    if (lockup && ((priv->stats.tx_packets + 1) % lockup) == 0) {
            /* Simulate a dropped transmit interrupt */
        netif_stop_queue(dev);
        pr_info("SNULL: Simulate lockup at %ld, txp %ld\n", jiffies,
                (unsigned long) priv->stats.tx_packets);
    }
    else
        snull_interrupt(0, dev, NULL);
}


int __snull_tx(struct sk_buff *skb, struct net_device *dev)
{
    int len;
    char *data, shortpkt[ETH_ZLEN];
    struct snull_priv *priv = netdev_priv(dev);

    data = skb->data;
    len = skb->len;
    if (len < ETH_ZLEN) {
        memset(shortpkt, 0, ETH_ZLEN);
        memcpy(shortpkt, skb->data, skb->len);
        len = ETH_ZLEN;
        data = shortpkt;
    }
    netif_trans_update(dev);

    /* Remember the skb, so we can free it at interrupt time */
    priv->skb = skb;

    /* actual deliver of data is device-specific, and not shown here */
    snull_hw_tx(data, len, dev);

    return 0; /* Our simple device can not fail */
}


void __snull_tx_timeout (struct net_device *dev, unsigned int txqueue)
{
    struct snull_priv *priv = netdev_priv(dev);
        struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

    pr_info("SNULL: Transmit timeout at %ld, latency %ld\n", jiffies,
            jiffies - txq->trans_start);
        /* Simulate a transmission interrupt to get things moving */
    priv->status |= SNULL_TX_INTR;
    snull_interrupt(0, dev, NULL);
    priv->stats.tx_errors++;

    /* Reset packet pool */
    spin_lock(&priv->lock);
    snull_teardown_pool(dev);
    snull_setup_pool(dev);
    spin_unlock(&priv->lock);

    netif_wake_queue(dev);
    return;
}


int __snull_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
    pr_info("SNULL: ioctl was called\n");
    return 0;
}


struct net_device_stats *__snull_stats(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    return &priv->stats;
}

/*
 * This function is called to fill up an eth header, since arp is not
 * available on the interface
 */
int snull_rebuild_header(struct sk_buff *skb)
{
    struct ethhdr *eth = (struct ethhdr *) skb->data;
    struct net_device *dev = skb->dev;
    
    memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
    memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);
    eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
    return 0;
}


int snull_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
    struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

    eth->h_proto = htons(type);
    memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
    memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
    eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
    return (dev->hard_header_len);
}


int __snull_change_mtu(struct net_device *dev, int new_mtu)
{
    unsigned long flags;
    struct snull_priv *priv = netdev_priv(dev);
    spinlock_t *lock = &priv->lock;
    
    /* check ranges */
    if ((new_mtu < 68) || (new_mtu > 1500))
        return -EINVAL;
    /*
     * Do anything you need, and the accept the value
     */
    spin_lock_irqsave(lock, flags);
    dev->mtu = new_mtu;
    spin_unlock_irqrestore(lock, flags);
    return 0; /* success */
}


static const struct header_ops snull_header_ops = {
        .create  = snull_header,
};


static const struct net_device_ops snull_netdev_ops = {
    .ndo_open            = __snull_open,
    .ndo_stop            = __snull_release,
    .ndo_start_xmit      = __snull_tx,
    .ndo_do_ioctl        = __snull_ioctl,
    .ndo_set_config      = __snull_config,
    .ndo_get_stats       = __snull_stats,
    .ndo_change_mtu      = __snull_change_mtu,
    .ndo_tx_timeout      = __snull_tx_timeout,
};

/*
 * The init function (sometimes called probe).
 * It is invoked by register_netdev()
 */
void snull_init(struct net_device *dev)
{
    struct snull_priv *priv;
    ether_setup(dev); /* assign some of the fields */
    dev->watchdog_timeo = timeout;
    dev->netdev_ops = &snull_netdev_ops;
    dev->header_ops = &snull_header_ops;
    /* keep the default flags, just add NOARP */
    dev->flags           |= IFF_NOARP;
    dev->features        |= NETIF_F_HW_CSUM;

    /*
     * Then, initialize the priv field. This encloses the statistics
     * and a few private fields.
     */
    priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct snull_priv));
    if (use_napi) {
        netif_napi_add(dev, &priv->napi, snull_poll);
    }
    spin_lock_init(&priv->lock);
    priv->dev = dev;

    snull_rx_ints(dev, 1);		/* enable receive interrupts */
    snull_setup_pool(dev);
}


int snull_add_device(void)
{
    int result, i, ret = -ENOMEM;

    snull_interrupt = use_napi ? snull_napi_interrupt : snull_regular_interrupt;

    /* Allocate the devices */
    snull_devs[0] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
            NET_NAME_UNKNOWN, snull_init);
    snull_devs[1] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
            NET_NAME_UNKNOWN, snull_init);
    if (snull_devs[0] == NULL || snull_devs[1] == NULL)
        goto out;

    ret = -ENODEV;
    for (i = 0; i < 2;  i++)
        if ((result = register_netdev(snull_devs[i])))
            pr_err("SNULL: error %i registering device \"%s\"\n",
                    result, snull_devs[i]->name);
        else
            ret = 0;
    out:
    if (ret) 
        snull_remove_device();
    return ret;
}

void snull_remove_device(void)
{
    int i;
    
    for (i = 0; i < 2;  i++) {
        if (snull_devs[i]) {
            unregister_netdev(snull_devs[i]);
            snull_teardown_pool(snull_devs[i]);
            free_netdev(snull_devs[i]); //will call netif_napi_del()
        }
    }
    return;
}