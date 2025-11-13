var group__DRV__LIN__HLD__MODULE =
[
    [ "sci_lin.h", "sci__lin_8h.html", null ],
    [ "LIN_HwAttrs", "structLIN__HwAttrs.html", [
      [ "baseAddr", "structLIN__HwAttrs.html#a23835bcdc939313ba3ab407b42589f98", null ],
      [ "intrNum0", "structLIN__HwAttrs.html#a9103951d959ed62ea2302d25db29bc84", null ],
      [ "intrNum1", "structLIN__HwAttrs.html#a1f2ae75cf63d583de1498f703f529238", null ],
      [ "opMode", "structLIN__HwAttrs.html#abce59f7449c4f6b1538167f1f20f617e", null ],
      [ "intrPriority", "structLIN__HwAttrs.html#a1f3475ca30774179931c4be1f3df1811", null ],
      [ "debugMode", "structLIN__HwAttrs.html#a545d11c5b12af98da0f30fdc43bb9d37", null ],
      [ "linClk", "structLIN__HwAttrs.html#a816ef58cb33ee1d8f8e7416d310b11d1", null ],
      [ "enableLoopback", "structLIN__HwAttrs.html#a594d0fefc6ed5580c760a1f0516d2809", null ],
      [ "loopBackMode", "structLIN__HwAttrs.html#aaefb44ec0d8615d1de7e9d5c5da023a5", null ],
      [ "loopBackType", "structLIN__HwAttrs.html#a60911ead02b7782a61b6f580134556e9", null ]
    ] ],
    [ "LIN_BaudConfigParams", "structLIN__BaudConfigParams.html", [
      [ "preScaler", "structLIN__BaudConfigParams.html#abaf30b0a91b1b5f193773ae57d82fcb3", null ],
      [ "fracDivSel_M", "structLIN__BaudConfigParams.html#add27a1a753fda95e68c408833e37ba42", null ],
      [ "supFracDivSel_U", "structLIN__BaudConfigParams.html#af2a231e8f4e34697809295bc11c6af90", null ]
    ] ],
    [ "LIN_SCI_Frame", "structLIN__SCI__Frame.html", [
      [ "id", "structLIN__SCI__Frame.html#a68c2a5f3b10baf39914f61dda31d90a8", null ],
      [ "frameLen", "structLIN__SCI__Frame.html#aca7591ff6722f33c332304ce46f08a10", null ],
      [ "dataBuf", "structLIN__SCI__Frame.html#a4f515fd9eb9f75b1eae3ac9d4a0388f5", null ],
      [ "txnType", "structLIN__SCI__Frame.html#a11daddb55baa8e76970ea3e6c49637e6", null ],
      [ "timeout", "structLIN__SCI__Frame.html#ab470e4ce7c11cc5bfb0da3f5ac80b249", null ],
      [ "status", "structLIN__SCI__Frame.html#ab809dcabdc6dd249ea7c41c6942f50a9", null ],
      [ "args", "structLIN__SCI__Frame.html#af2bfb41f9bd3164e9bfdf01d3811e03c", null ]
    ] ],
    [ "LIN_SciConfigParams", "structLIN__SciConfigParams.html", [
      [ "parityType", "structLIN__SciConfigParams.html#adaef92065de32c9894b3b3cfa3281a80", null ],
      [ "stopBits", "structLIN__SciConfigParams.html#a278842e929dca1a413c4e51583cc978a", null ],
      [ "dataBits", "structLIN__SciConfigParams.html#a0ede3cc8c5cff8649948f83feb7c28b5", null ],
      [ "transferCompleteCallbackFxn", "structLIN__SciConfigParams.html#a3e61e4f77e0ad848acf5390499bc0ff6", null ]
    ] ],
    [ "LIN_LinConfigParams", "structLIN__LinConfigParams.html", [
      [ "linMode", "structLIN__LinConfigParams.html#a155825a6920fbbb3811474ddbea25e05", null ],
      [ "maskFilteringType", "structLIN__LinConfigParams.html#a051ec1886993c2140d98a9c257966ccf", null ],
      [ "linTxMask", "structLIN__LinConfigParams.html#a7b5ed0e034ac74e526a5c9df27a0fb22", null ],
      [ "linRxMask", "structLIN__LinConfigParams.html#a517a0276648152f3b2b1d3661985747a", null ],
      [ "checksumType", "structLIN__LinConfigParams.html#a6f1a597dafaf6907d4f52441d3b053c5", null ],
      [ "adaptModeEnable", "structLIN__LinConfigParams.html#a7ac3f10f14386d9ed38215942f6f07a8", null ],
      [ "maxBaudRate", "structLIN__LinConfigParams.html#ac621ef1eb13012505728dd7a5d2b8b8a", null ],
      [ "syncDelimiter", "structLIN__LinConfigParams.html#a460a7ff2b5ba993d1ab9227950f68b87", null ],
      [ "syncBreak", "structLIN__LinConfigParams.html#a5538d6fe83e03a9e65be7b7bae939b5e", null ],
      [ "idMatchCallbackFxn", "structLIN__LinConfigParams.html#a08fb76fbc756d449809ce4defa749148", null ],
      [ "transferCompleteCallbackFxn", "structLIN__LinConfigParams.html#a970f8efbe4ef8c322397e08b68627ef2", null ]
    ] ],
    [ "LIN_OpenParams", "structLIN__OpenParams.html", [
      [ "transferMode", "structLIN__OpenParams.html#a0e59ca356da25be67c021ea7e3a7dbd8", null ],
      [ "moduleMode", "structLIN__OpenParams.html#a0665ede5c14f1f7df52d1e8a4ac5cfb6", null ],
      [ "enableParity", "structLIN__OpenParams.html#a997e0169ab6dfa14a39c5bf83f54711c", null ],
      [ "commMode", "structLIN__OpenParams.html#a497a9bc703d28c94aa865e8bb36c03a7", null ],
      [ "multiBufferMode", "structLIN__OpenParams.html#afcb13ed2ff2223e3df0178abefa08857", null ],
      [ "baudConfigParams", "structLIN__OpenParams.html#a095fcf89d424dbd5ba8ce622e11ce28f", null ],
      [ "sciConfigParams", "structLIN__OpenParams.html#a881c53e8a01c5520e60faecb0451bf4c", null ],
      [ "linConfigParams", "structLIN__OpenParams.html#a53b2327a846be9d1dc6e7c4879b71166", null ]
    ] ],
    [ "LIN_Object", "structLIN__Object.html", [
      [ "__attribute__", "structLIN__Object.html#a81748a9ee624cec7d6cdb0a56f7abc50", null ],
      [ "__attribute__", "structLIN__Object.html#a15cbff4424623a78c5bb729922fdef5f", null ],
      [ "__attribute__", "structLIN__Object.html#aee63822abf1236411aa82e160cec72d9", null ],
      [ "mutex", "structLIN__Object.html#adcd2093330dd988800ae48e90baf8513", null ],
      [ "hwiObj0", "structLIN__Object.html#ab1966bac6a7d83c845315b876fe26c77", null ],
      [ "hwiObj1", "structLIN__Object.html#ac33a008ba97e2e1951bfc7faa2e58e28", null ],
      [ "state", "structLIN__Object.html#a34c747747450365ce86aeac01f2b12d4", null ],
      [ "isOpen", "structLIN__Object.html#af3d88888872f90c12aea0e40de5e13c8", null ],
      [ "openParams", "structLIN__Object.html#a60566f5388f7ccc339cc3bf3b99d30cc", null ],
      [ "linDmaHandle", "structLIN__Object.html#a369ee2383dd9c3abf3d37f96ae0884c5", null ],
      [ "dmaChCfg", "structLIN__Object.html#a3abfc82f7b8f35def3a49da43a42a4d9", null ],
      [ "currentTxnFrame", "structLIN__Object.html#a98574535b3437733a295fae0fd314f3e", null ],
      [ "writeBufIdx", "structLIN__Object.html#a306cc8ffdc409b9444fa160d1d7c7caf", null ],
      [ "writeCountIdx", "structLIN__Object.html#a9bf8ef34ca9092f22287155463d57359", null ],
      [ "writeFrmCompSemObj", "structLIN__Object.html#aca0ebd508eb3699b43a846249ebd80a0", null ],
      [ "readFrmCompSemObj", "structLIN__Object.html#a80e15c7dab47ddf6584b6e71f335d530", null ]
    ] ],
    [ "LIN_Config", "structLIN__Config.html", [
      [ "object", "structLIN__Config.html#a77ace6906c5402155b3675e1bc3a5d01", null ],
      [ "hwAttrs", "structLIN__Config.html#ad04200eabd60c596feac0779aae07ced", null ]
    ] ],
    [ "LIN_Handle", "group__DRV__LIN__HLD__MODULE.html#ga0b6a156cd36aaa4916fd54d48f741a3f", null ],
    [ "LIN_DmaChConfig", "group__DRV__LIN__HLD__MODULE.html#gaf727c4f1c348113516cb3150846b1311", null ],
    [ "LIN_DmaHandle", "group__DRV__LIN__HLD__MODULE.html#gab059c50303a6ae761a780cc9dbe8403f", null ],
    [ "LIN_IdMatchCallbackFxn", "group__DRV__LIN__HLD__MODULE.html#ga50feff8726dcf8b8ab5be2bea36bab65", null ],
    [ "LIN_TransferCompleteCallbackFxn", "group__DRV__LIN__HLD__MODULE.html#ga189982c34bb8edd9240b3392807b6482", null ],
    [ "LIN_OperationalMode", "group__DRV__LIN__HLD__MODULE.html#gafc048b1cc238ef7ac721bac6287c4a46", [
      [ "LIN_OPER_MODE_POLLING", "group__DRV__LIN__HLD__MODULE.html#ggafc048b1cc238ef7ac721bac6287c4a46a9eb940150b1b986abc70fcf6d774447b", null ],
      [ "LIN_OPER_MODE_INTERRUPT", "group__DRV__LIN__HLD__MODULE.html#ggafc048b1cc238ef7ac721bac6287c4a46ad066434ce06ea84455c42c7ba2e44991", null ],
      [ "LIN_OPER_MODE_DMA", "group__DRV__LIN__HLD__MODULE.html#ggafc048b1cc238ef7ac721bac6287c4a46a8b6ee6b4ee34998b6be7b191bd5b475c", null ]
    ] ],
    [ "LIN_TransferMode", "group__DRV__LIN__HLD__MODULE.html#gacc42f96427a7434e0718cdbebf382242", [
      [ "LIN_TRANSFER_MODE_BLOCKING", "group__DRV__LIN__HLD__MODULE.html#ggacc42f96427a7434e0718cdbebf382242aec419ba49db7ba336c9e1c22018e19c8", null ],
      [ "LIN_TRANSFER_MODE_CALLBACK", "group__DRV__LIN__HLD__MODULE.html#ggacc42f96427a7434e0718cdbebf382242a98d30cea17da0d5851485ce6c911d913", null ]
    ] ],
    [ "LIN_HLD_ModuleMode", "group__DRV__LIN__HLD__MODULE.html#ga641c81ac03a88131af952351a210ed68", [
      [ "LIN_MODULE_OP_MODE_LIN", "group__DRV__LIN__HLD__MODULE.html#gga641c81ac03a88131af952351a210ed68adf1ff15baf0ab720433c0fc7621785b2", null ],
      [ "LIN_MODULE_OP_MODE_SCI", "group__DRV__LIN__HLD__MODULE.html#gga641c81ac03a88131af952351a210ed68a20d29f27ddf1529c77b86a8256892009", null ]
    ] ],
    [ "LIN_HLD_LinMode", "group__DRV__LIN__HLD__MODULE.html#ga992c7796231a9876774d552f2cd91e5f", [
      [ "LIN_MODE_HLD_LIN_COMMANDER", "group__DRV__LIN__HLD__MODULE.html#gga992c7796231a9876774d552f2cd91e5fa9df58375a9ddfdbafce1c46eb128db81", null ],
      [ "LIN_MODE_HLD_LIN_RESPONDER", "group__DRV__LIN__HLD__MODULE.html#gga992c7796231a9876774d552f2cd91e5fa9190d772195d761a7dad739fa4b01072", null ]
    ] ],
    [ "LIN_HLD_CommMode", "group__DRV__LIN__HLD__MODULE.html#ga9d610fb4c51e5d8eb3ef68ba713a6386", [
      [ "LIN_COMM_HLD_LIN_ID4ID5LENCTL", "group__DRV__LIN__HLD__MODULE.html#gga9d610fb4c51e5d8eb3ef68ba713a6386a7df391549532cdd9647ab24a7b153ebc", null ],
      [ "LIN_COMM_HLD_LIN_USELENGTHVAL", "group__DRV__LIN__HLD__MODULE.html#gga9d610fb4c51e5d8eb3ef68ba713a6386a243b5e19c4348c65d2b50a9e77647093", null ],
      [ "LIN_COMM_HLD_SCI_ADDRBITMODE", "group__DRV__LIN__HLD__MODULE.html#gga9d610fb4c51e5d8eb3ef68ba713a6386aaa6eb8a3608f8ec93faf13aa0c007215", null ],
      [ "LIN_COMM_HLD_SCI_IDLELINEMODE", "group__DRV__LIN__HLD__MODULE.html#gga9d610fb4c51e5d8eb3ef68ba713a6386af1de003489a11266c446a1476ae60098", null ]
    ] ],
    [ "LIN_HLD_DebugMode", "group__DRV__LIN__HLD__MODULE.html#gabcf18973416f051c0b6a8df8b6226504", [
      [ "LIN_HLD_DEBUG_FROZEN", "group__DRV__LIN__HLD__MODULE.html#ggabcf18973416f051c0b6a8df8b6226504a99adece3addb67abcbfdd5b0658e64f1", null ],
      [ "LIN_HLD_DEBUG_COMPLETE", "group__DRV__LIN__HLD__MODULE.html#ggabcf18973416f051c0b6a8df8b6226504a8cc0954e5f6f1fe86d897bad09b98c52", null ]
    ] ],
    [ "LIN_HLD_ChecksumType", "group__DRV__LIN__HLD__MODULE.html#gab88b22501f50378afb3326c817fcfe01", [
      [ "LIN_HLD_CHECKSUM_CLASSIC", "group__DRV__LIN__HLD__MODULE.html#ggab88b22501f50378afb3326c817fcfe01a5f1498dfd1fe16a02fd7942588fd89c2", null ],
      [ "LIN_HLD_CHECKSUM_ENHANCED", "group__DRV__LIN__HLD__MODULE.html#ggab88b22501f50378afb3326c817fcfe01aea15efc8b229d2e36d4be26ebf73f19d", null ]
    ] ],
    [ "LIN_HLD_MaskFilterType", "group__DRV__LIN__HLD__MODULE.html#ga50e7f33cf3d7fb0873d89c441e5efb42", [
      [ "LIN_HLD_MSG_FILTER_IDBYTE", "group__DRV__LIN__HLD__MODULE.html#gga50e7f33cf3d7fb0873d89c441e5efb42afd5213b6040d2e1c3ed4b81be4038b48", null ],
      [ "LIN_HLD_MSG_FILTER_IDRESPONDER", "group__DRV__LIN__HLD__MODULE.html#gga50e7f33cf3d7fb0873d89c441e5efb42a21c8f464ebc2c3a6e070435f15c1899d", null ]
    ] ],
    [ "LIN_HLD_SCIParityType", "group__DRV__LIN__HLD__MODULE.html#ga920a4d70e198d679ec6dbf17a1e2d227", [
      [ "LIN_HLD_SCI_PARITY_ODD", "group__DRV__LIN__HLD__MODULE.html#gga920a4d70e198d679ec6dbf17a1e2d227ab9c1e49842ac6733b1548b07e2bd8e26", null ],
      [ "LIN_HLD_SCI_PARITY_EVEN", "group__DRV__LIN__HLD__MODULE.html#gga920a4d70e198d679ec6dbf17a1e2d227a88a59db284e12d91fc73781c16d43384", null ]
    ] ],
    [ "LIN_HLD_SCIStopBits", "group__DRV__LIN__HLD__MODULE.html#ga5e2bb9e72ccf04eb386065cd4016bf88", [
      [ "LIN_HLD_SCI_STOP_BITS_1", "group__DRV__LIN__HLD__MODULE.html#gga5e2bb9e72ccf04eb386065cd4016bf88aa16a977d81930f91cc7e272a17c1447b", null ],
      [ "LIN_HLD_SCI_STOP_BITS_2", "group__DRV__LIN__HLD__MODULE.html#gga5e2bb9e72ccf04eb386065cd4016bf88a6ac8dc25c5d6bf274ab42ebcf001ae7b", null ]
    ] ],
    [ "LIN_HLD_LoopbackMode", "group__DRV__LIN__HLD__MODULE.html#gad2f4fb92a06a8509d770a3efe363b569", [
      [ "LIN_HLD_LOOPBACK_INTERNAL", "group__DRV__LIN__HLD__MODULE.html#ggad2f4fb92a06a8509d770a3efe363b569a7c2a85eb81fbfce1cf25221de4323f39", null ],
      [ "LIN_HLD_LOOPBACK_EXTERNAL", "group__DRV__LIN__HLD__MODULE.html#ggad2f4fb92a06a8509d770a3efe363b569ac0dcd0d6116374e539d3aff454df57ce", null ]
    ] ],
    [ "LIN_HLD_LoopbackType", "group__DRV__LIN__HLD__MODULE.html#ga9cc61d62d1a27e74e96c0fd92ce911b2", [
      [ "LIN_HLD_LOOPBACK_DIGITAL", "group__DRV__LIN__HLD__MODULE.html#gga9cc61d62d1a27e74e96c0fd92ce911b2aba5fdcf61120b46e550fe8a17959c9e1", null ],
      [ "LIN_HLD_LOOPBACK_ANALOG", "group__DRV__LIN__HLD__MODULE.html#gga9cc61d62d1a27e74e96c0fd92ce911b2a51d53fc25b0c0e8764f016302df03279", null ]
    ] ],
    [ "LIN_HLD_LoopbackPath", "group__DRV__LIN__HLD__MODULE.html#ga500806f07111f2b2b57cc92c5e5f9b34", [
      [ "LIN_HLD_ANALOG_LOOP_TX", "group__DRV__LIN__HLD__MODULE.html#gga500806f07111f2b2b57cc92c5e5f9b34a7550d30a902051a002f3184a57e54169", null ],
      [ "LIN_HLD_ANALOG_LOOP_RX", "group__DRV__LIN__HLD__MODULE.html#gga500806f07111f2b2b57cc92c5e5f9b34a8313858c680fbd1ce14a5f4f1a7b3af4", null ]
    ] ],
    [ "LIN_HLD_Sync_Delimiter", "group__DRV__LIN__HLD__MODULE.html#gac490370df0c9f49cdaf783fb61ef0d57", [
      [ "LIN_HLD_SYNC_DELIMITER_LEN_1", "group__DRV__LIN__HLD__MODULE.html#ggac490370df0c9f49cdaf783fb61ef0d57aae12f53754b216f93612982f95e8dd2b", null ],
      [ "LIN_HLD_SYNC_DELIMITER_LEN_2", "group__DRV__LIN__HLD__MODULE.html#ggac490370df0c9f49cdaf783fb61ef0d57ad9da3906bad94396ff387b7b452f3ee0", null ],
      [ "LIN_HLD_SYNC_DELIMITER_LEN_3", "group__DRV__LIN__HLD__MODULE.html#ggac490370df0c9f49cdaf783fb61ef0d57a1d80d87aee918c8bd7c621b3994480c2", null ],
      [ "LIN_HLD_SYNC_DELIMITER_LEN_4", "group__DRV__LIN__HLD__MODULE.html#ggac490370df0c9f49cdaf783fb61ef0d57aacfd79dca101a77f4cb7c046a4febd67", null ]
    ] ],
    [ "LIN_HLD_Sync_Break", "group__DRV__LIN__HLD__MODULE.html#ga0ae14d80b70437e4211dd72dbee8445f", [
      [ "LIN_HLD_SYNC_BREAK_LEN_13", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fa6da223cd7886de320965f6adaa8fb9d7", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_14", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fa78e88d4d947eb50f58c97b5b003edb6e", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_15", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fae345b9e297850c7721a1430aa5c95f3c", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_16", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fa42d5c229e4232dbc6dac59dd1dd20b62", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_17", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445faaf835791b9a5e10fe1de0f47e09e4441", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_18", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fa760592d2d592b35bbdecee6d6b706a28", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_19", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fa7d83c71362c60a8dcd320eae1994a638", null ],
      [ "LIN_HLD_SYNC_BREAK_LEN_20", "group__DRV__LIN__HLD__MODULE.html#gga0ae14d80b70437e4211dd72dbee8445fac9b6fd447a6fc5fbcd0839867c15afab", null ]
    ] ],
    [ "LIN_HLD_Txn_Status", "group__DRV__LIN__HLD__MODULE.html#gae7353ac18b190c80ce680466b294c695", [
      [ "LIN_HLD_TXN_STS_SUCCESS", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a580cd33095ea5021d36524c87884f47c", null ],
      [ "LIN_HLD_TXN_STS_FAILURE", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a67fe1a84dc163f891d3830f8a27813e0", null ],
      [ "LIN_HLD_TXN_STS_TIMEOUT", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a5a267c76e7e3e62bd7492278ff5e3def", null ],
      [ "LIN_HLD_TXN_PHY_BUS_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695ab5243a85c6f40c7da867266bd5500c51", null ],
      [ "LIN_HLD_TXN_FRAMING_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695ac5be14b37ecc85068d2b7950fe867fb9", null ],
      [ "LIN_HLD_TXN_OVERRUN_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695afb9b314b1958341a296da5e71ba35f00", null ],
      [ "LIN_HLD_TXN_PARITY_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a433fef8e6dcef89d9d180be9183ea74d", null ],
      [ "LIN_HLD_TXN_CHECKSUM_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a615f51d3f0efec87b3028b19831b3f65", null ],
      [ "LIN_HLD_TXN_NO_RES_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695abac158642af1c0c667c611cbb1e1fb1a", null ],
      [ "LIN_HLD_TXN_BIT_ERR", "group__DRV__LIN__HLD__MODULE.html#ggae7353ac18b190c80ce680466b294c695a161744a5cfba1ad07922b9881703ebd2", null ]
    ] ],
    [ "LIN_HLD_Txn_Type", "group__DRV__LIN__HLD__MODULE.html#ga6a63b96b57591efe682507263db5f3d1", [
      [ "LIN_HLD_TXN_TYPE_WRITE", "group__DRV__LIN__HLD__MODULE.html#gga6a63b96b57591efe682507263db5f3d1ae262d8976fbc4c1cf31dd2606e97d541", null ],
      [ "LIN_HLD_TXN_TYPE_READ", "group__DRV__LIN__HLD__MODULE.html#gga6a63b96b57591efe682507263db5f3d1ace417f29c90b96b520751814c8adf8aa", null ]
    ] ],
    [ "LIN_HLD_State", "group__DRV__LIN__HLD__MODULE.html#gad8b9bde6da71ea6ff81e51846dcee3b6", [
      [ "LIN_HLD_STATE_RESET", "group__DRV__LIN__HLD__MODULE.html#ggad8b9bde6da71ea6ff81e51846dcee3b6aaa66936659431ce203ac729cd5542e32", null ],
      [ "LIN_HLD_STATE_IDLE", "group__DRV__LIN__HLD__MODULE.html#ggad8b9bde6da71ea6ff81e51846dcee3b6a59e42ec1e277f78d9494a682bdf344df", null ],
      [ "LIN_HLD_STATE_BUSY", "group__DRV__LIN__HLD__MODULE.html#ggad8b9bde6da71ea6ff81e51846dcee3b6ad35dcb4431b54ff2393480199e58571a", null ],
      [ "LIN_HLD_STATE_ERROR", "group__DRV__LIN__HLD__MODULE.html#ggad8b9bde6da71ea6ff81e51846dcee3b6a9384f3ae3abc11345a6acb0418a5211b", null ]
    ] ],
    [ "LIN_init", "group__DRV__LIN__HLD__MODULE.html#gacfcf77a9e16f937b34f9961a9a148bdf", null ],
    [ "LIN_deinit", "group__DRV__LIN__HLD__MODULE.html#ga81e7fe50fd9726ee7dd90c0fa1309d34", null ],
    [ "LIN_Params_init", "group__DRV__LIN__HLD__MODULE.html#gac602e11cb5e3dcf7ebd44a7d45fe9a10", null ],
    [ "LIN_open", "group__DRV__LIN__HLD__MODULE.html#ga3ef5405fe115a697caa2961e9e423288", null ],
    [ "LIN_close", "group__DRV__LIN__HLD__MODULE.html#ga31e5d0d27e525b49358153457ac1a454", null ],
    [ "LIN_SCI_Frame_init", "group__DRV__LIN__HLD__MODULE.html#ga903b37581491d68e58803593a798e6f7", null ],
    [ "LIN_SCI_transferFrame", "group__DRV__LIN__HLD__MODULE.html#ga7b7ee26200bc35058069788a2de93199", null ],
    [ "LIN_getHandle", "group__DRV__LIN__HLD__MODULE.html#ga06bdedee1ba51e133886c75068349a13", null ]
];