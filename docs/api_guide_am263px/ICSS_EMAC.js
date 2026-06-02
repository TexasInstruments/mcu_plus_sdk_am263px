var ICSS_EMAC =
[
    [ "Introduction", "ICSS_EMAC.html#autotoc_md1438", null ],
    [ "Features Supported", "ICSS_EMAC.html#autotoc_md1439", null ],
    [ "SysConfig Features", "ICSS_EMAC.html#ICSS_EMAC_SYSCONFIG_FEATURES", null ],
    [ "Features not supported", "ICSS_EMAC.html#autotoc_md1440", null ],
    [ "Terms and Abbreviations", "ICSS_EMAC.html#autotoc_md1441", null ],
    [ "ICSS-EMAC Design", "ICSS_EMAC.html#autotoc_md1442", null ],
    [ "ICSS-EMAC Queue Management Design", "ICSS_EMAC.html#autotoc_md1443", null ],
    [ "Usage", "ICSS_EMAC.html#autotoc_md1444", [
      [ "Enable ICSS-EMAC in SysConfig", "ICSS_EMAC.html#autotoc_md1445", null ],
      [ "Update linker command file", "ICSS_EMAC.html#autotoc_md1446", null ],
      [ "Update MPU for the CPU", "ICSS_EMAC.html#autotoc_md1447", null ],
      [ "Including the header file", "ICSS_EMAC.html#autotoc_md1448", null ],
      [ "Initializing the Handle", "ICSS_EMAC.html#autotoc_md1449", null ],
      [ "Sending a Packet", "ICSS_EMAC.html#autotoc_md1450", null ],
      [ "Receiving a Packet", "ICSS_EMAC.html#autotoc_md1451", null ],
      [ "IOCTL", "ICSS_EMAC.html#autotoc_md1452", null ]
    ] ],
    [ "Dependencies", "ICSS_EMAC.html#autotoc_md1453", null ],
    [ "Debug Guide", "ICSS_EMAC.html#autotoc_md1454", null ],
    [ "API", "ICSS_EMAC.html#autotoc_md1455", null ],
    [ "Dual EMAC and Switch Firmwares", "ICSS_EMAC.html#autotoc_md1456", null ],
    [ "ICSS-EMAC Design", "ICSS_EMAC_DESIGN.html", [
      [ "Modes of Operation", "ICSS_EMAC_DESIGN.html#autotoc_md1464", null ],
      [ "Ports in ICSS-EMAC Context", "ICSS_EMAC_DESIGN.html#autotoc_md1465", null ],
      [ "Memory Map", "ICSS_EMAC_DESIGN.html#autotoc_md1466", [
        [ "Queue Buffers", "ICSS_EMAC_DESIGN.html#autotoc_md1467", null ],
        [ "Shared Data RAM", "ICSS_EMAC_DESIGN.html#autotoc_md1468", null ],
        [ "PRU0 Data RAM", "ICSS_EMAC_DESIGN.html#autotoc_md1469", null ],
        [ "PRU1 Data RAM", "ICSS_EMAC_DESIGN.html#autotoc_md1470", null ]
      ] ],
      [ "Quality of Service and Queues", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_QOS", null ],
      [ "Data Path", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_DATA_PATH", [
        [ "Rx Data Path", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_DATA_PATH_RX", null ],
        [ "Tx Data Path", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_DATA_PATH_TX", null ],
        [ "Forwarding Rules", "ICSS_EMAC_DESIGN.html#autotoc_md1471", null ]
      ] ],
      [ "OS Components", "ICSS_EMAC_DESIGN.html#autotoc_md1472", [
        [ "Interrupts", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_INTERRUPTS", null ],
        [ "Tasks", "ICSS_EMAC_DESIGN.html#autotoc_md1473", null ],
        [ "Semaphores", "ICSS_EMAC_DESIGN.html#autotoc_md1474", null ]
      ] ],
      [ "Interrupt Pacing", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_INTERRUPT_PACING", null ],
      [ "Half Duplex Support", "ICSS_EMAC_DESIGN.html#autotoc_md1475", null ],
      [ "Learning/FDB", "ICSS_EMAC_DESIGN.html#autotoc_md1476", [
        [ "Usage", "ICSS_EMAC_DESIGN.html#autotoc_md1477", null ]
      ] ],
      [ "Storm Prevention", "ICSS_EMAC_DESIGN.html#autotoc_md1478", [
        [ "Usage", "ICSS_EMAC_DESIGN.html#autotoc_md1479", null ]
      ] ],
      [ "Statistics", "ICSS_EMAC_DESIGN.html#ICSS_EMAC_DESIGN_STATISTICS", [
        [ "Usage", "ICSS_EMAC_DESIGN.html#autotoc_md1480", null ]
      ] ]
    ] ],
    [ "ICSS EMAC Queue Management Design", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html", [
      [ "Overview", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1481", null ],
      [ "Ports in ICSS EMAC", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1482", null ],
      [ "Port-Based Queue Organization", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1483", [
        [ "Queue Distribution", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1484", null ],
        [ "Default Queue Sizes", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1485", null ]
      ] ],
      [ "Introduction to Queue Management Units", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1486", [
        [ "Buffer", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1487", null ],
        [ "Buffer Descriptor (BD)", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1488", null ],
        [ "Queue", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1489", null ],
        [ "Queue Descriptor (QD)", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1490", null ]
      ] ],
      [ "Detailed Component Structure and Relationships", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1491", null ],
      [ "Collision Queue", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1492", [
        [ "What is Queue Contention?", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1493", null ],
        [ "Primary-Secondary Arbitration System", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1494", null ],
        [ "How the Arbitration Works", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1495", null ],
        [ "Collision Queue Mechanism", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1496", null ]
      ] ],
      [ "Reception Flow Example", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1497", [
        [ "Initial State (Empty Queues)", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1498", null ],
        [ "Packet Arrives at PRU (Firmware Receives)", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1499", null ],
        [ "Firmware (Producer) Actions", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1500", null ],
        [ "Host (Consumer) Actions", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1501", null ]
      ] ],
      [ "Transmission Flow Example", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1502", [
        [ "Initial State (Empty Transmit Queue)", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1503", null ],
        [ "Application Calls ICSS_EMAC_txPacket", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1504", null ],
        [ "Driver (Producer) Actions in ICSS_EMAC_txPacketEnqueue", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1505", null ],
        [ "Firmware (Consumer) Actions", "ICSS_EMAC_QUEUE_MANAGEMENT_DESIGN.html#autotoc_md1506", null ]
      ] ]
    ] ],
    [ "ICSS-EMAC Debug Guide", "ICSS_EMAC_DEBUG_GUIDE.html", [
      [ "Assumption", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1457", null ],
      [ "Scope", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1458", null ],
      [ "Common Debugging Tasks", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1459", [
        [ "Loading and running on CCS", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1460", null ],
        [ "Checking Link Status", "ICSS_EMAC_DEBUG_GUIDE.html#ICSS_EMAC_DEBUG_GUIDE_CHECKING_LINK_STATUS", null ],
        [ "Checking if Receive is working", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1461", null ],
        [ "Checking if Transmit is working", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1462", null ],
        [ "Checking Statistics", "ICSS_EMAC_DEBUG_GUIDE.html#ICSS_EMAC_DEBUG_GUIDE_CHECKING_STATISTICS", null ]
      ] ],
      [ "Accessing Memory", "ICSS_EMAC_DEBUG_GUIDE.html#ICSS_EMAC_DEBUG_GUIDE_ACCESSING_MEMORY", null ],
      [ "Using ROV to Debug RTOS", "ICSS_EMAC_DEBUG_GUIDE.html#autotoc_md1463", null ]
    ] ],
    [ "Dual EMAC and Switch", "DUAL_EMAC_AND_SWITCH.html", [
      [ "Introduction", "DUAL_EMAC_AND_SWITCH.html#autotoc_md1507", null ],
      [ "DUAL_EMAC", "DUAL_EMAC_AND_SWITCH.html#autotoc_md1508", null ],
      [ "SWITCH", "DUAL_EMAC_AND_SWITCH.html#autotoc_md1509", null ]
    ] ]
];