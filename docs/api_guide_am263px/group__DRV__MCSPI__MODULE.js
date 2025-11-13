var group__DRV__MCSPI__MODULE =
[
    [ "mcspi/v0/mcspi.h", "mcspi_2v0_2mcspi_8h.html", null ],
    [ "MCSPI_OpenParams", "structMCSPI__OpenParams.html", [
      [ "transferMode", "structMCSPI__OpenParams.html#a0b4ecaba7263bd18bc7e40924d8d6c97", null ],
      [ "transferTimeout", "structMCSPI__OpenParams.html#a1a61eb9a3dafb53eb367e31b41a7116b", null ],
      [ "transferCallbackFxn", "structMCSPI__OpenParams.html#a43a61b1bdcb52eebaf0d756f5e5500d8", null ],
      [ "msMode", "structMCSPI__OpenParams.html#ad539f2b8771de73167b55763b7dc984f", null ],
      [ "mcspiDmaIndex", "structMCSPI__OpenParams.html#a2ac3a45a175414a578667947ef548d18", null ]
    ] ],
    [ "MCSPI_Attrs", "structMCSPI__Attrs.html", [
      [ "baseAddr", "structMCSPI__Attrs.html#a75b1b1fa879c5f9e12d08da2cdc3c56d", null ],
      [ "inputClkFreq", "structMCSPI__Attrs.html#ad75f91618067bdb27aa9fddc6aec9f01", null ],
      [ "intrNum", "structMCSPI__Attrs.html#ae1472fd5de815ffaab70adb63473ed38", null ],
      [ "operMode", "structMCSPI__Attrs.html#a5230647329726edc89012fe9596c7162", null ],
      [ "intrPriority", "structMCSPI__Attrs.html#a359bf587e16e6aae894a3250ac20236c", null ],
      [ "chMode", "structMCSPI__Attrs.html#a4fb115e0add03fe92c8d615e7eae10f6", null ],
      [ "pinMode", "structMCSPI__Attrs.html#a1978d81ffd672b7414de333bb748ef54", null ],
      [ "initDelay", "structMCSPI__Attrs.html#a448f86e070dacb873eba3366c40e126b", null ],
      [ "multiWordAccess", "structMCSPI__Attrs.html#aed88676acb57223688f5bcf72cf6fd4a", null ]
    ] ],
    [ "MCSPI_Object", "structMCSPI__Object.html", [
      [ "handle", "structMCSPI__Object.html#a659c231d4718d09fa52824f5c608a1b4", null ],
      [ "openPrms", "structMCSPI__Object.html#a064cb9938bb0c93bc8cb84a509106140", null ],
      [ "baseAddr", "structMCSPI__Object.html#acadca04921a6f181499384fc8a5bddfd", null ],
      [ "chObj", "structMCSPI__Object.html#ac7da2dfd190c5dc49503556576dadab5", null ],
      [ "isOpen", "structMCSPI__Object.html#aa3e4c28312f82cb374ecc3f483f036fa", null ],
      [ "transferSem", "structMCSPI__Object.html#a769f011c0ac0d5bb8912c294a858c437", null ],
      [ "transferSemObj", "structMCSPI__Object.html#ad4c4c9072cfe2f98fb7cd5bddfe360eb", null ],
      [ "hwiHandle", "structMCSPI__Object.html#a7254f20c765f5ca0e1aaf301e5a96441", null ],
      [ "hwiObj", "structMCSPI__Object.html#ae39700175f11c0bbec9c3a0772d4f0f1", null ],
      [ "transaction", "structMCSPI__Object.html#a358d85e4cf82aea7204131610f830a8b", null ],
      [ "mcspiDmaHandle", "structMCSPI__Object.html#aca68ad9de1a0d77c7be021092674c33c", null ],
      [ "mcspiLldObject", "structMCSPI__Object.html#a0b51d3fbe7e9ce2b76007e6ebff177d3", null ],
      [ "mcspiLldHandle", "structMCSPI__Object.html#abe573a87994ff7083a0e78bf5e6b4986", null ],
      [ "mcspiLldInitObj", "structMCSPI__Object.html#aec7ad9f6ff0ef1a083ae6f338ffee66f", null ]
    ] ],
    [ "MCSPI_Config", "structMCSPI__Config.html", [
      [ "attrs", "structMCSPI__Config.html#a9855facd83ddb5bd31a11a08ec33a0b5", null ],
      [ "object", "structMCSPI__Config.html#a478cd61311b2ad7a75e83153901891fc", null ]
    ] ],
    [ "MCSPI_TRANSFER_MODE_BLOCKING", "group__DRV__MCSPI__MODULE.html#gab50fd130c0f0d29545a83be89f230b8e", null ],
    [ "MCSPI_TRANSFER_MODE_CALLBACK", "group__DRV__MCSPI__MODULE.html#gab2e9e948624aa8ff39cc5a75e00adb6f", null ],
    [ "MCSPI_Handle", "group__DRV__MCSPI__MODULE.html#gae5c48c3d1206a823e48b3180eeb9bca1", null ],
    [ "MCSPI_CallbackFxn", "group__DRV__MCSPI__MODULE.html#gaae5efefe555d07856ddea7034d5a40f4", null ],
    [ "MCSPI_init", "group__DRV__MCSPI__MODULE.html#ga7ab49f55fcfb89a487857e9a27ca7083", null ],
    [ "MCSPI_deinit", "group__DRV__MCSPI__MODULE.html#gaf34ea4d022337821d4c7066505726893", null ],
    [ "MCSPI_open", "group__DRV__MCSPI__MODULE.html#gabedb148b6832d0c132352b9c4fd0a923", null ],
    [ "MCSPI_close", "group__DRV__MCSPI__MODULE.html#ga12824149d7be9c07abd4cb120ec7cf13", null ],
    [ "MCSPI_getHandle", "group__DRV__MCSPI__MODULE.html#ga5c4aa6571c035b07058b12bb3520c980", null ],
    [ "MCSPI_chConfig", "group__DRV__MCSPI__MODULE.html#gaf55bfd5fe8741647f107cacba32a04e3", null ],
    [ "MCSPI_dmaChConfig", "group__DRV__MCSPI__MODULE.html#ga07c78f625ea6e766b3459bd350f35c9b", null ],
    [ "MCSPI_transfer", "group__DRV__MCSPI__MODULE.html#ga35061166ed36e50dc6d7b38a8fbf13e9", null ],
    [ "MCSPI_transferCancel", "group__DRV__MCSPI__MODULE.html#ga682d7e8ba62f079a7a463355e23d133b", null ],
    [ "MCSPI_OpenParams_init", "group__DRV__MCSPI__MODULE.html#ga8e2f6ba4de2ec70859f37324e6aa65ba", null ],
    [ "MCSPI_ChConfig_init", "group__DRV__MCSPI__MODULE.html#gab3158f44e2826e25f90111aa24453cc1", null ],
    [ "MCSPI_Transaction_init", "group__DRV__MCSPI__MODULE.html#ga427c74cdbf56cf1081b167728beb4a7f", null ],
    [ "MCSPI_reConfigFifo", "group__DRV__MCSPI__MODULE.html#ga3ee437e7c56248b80869d51d441fe4cd", null ],
    [ "MCSPI_getBaseAddr", "group__DRV__MCSPI__MODULE.html#gac52df4ca2575f0139fc603d069799073", null ],
    [ "gMcspiConfig", "group__DRV__MCSPI__MODULE.html#ga0fd285a89004e0eacc5a6be6c7136ea7", null ],
    [ "gMcspiConfigNum", "group__DRV__MCSPI__MODULE.html#ga09891815abcd4e3d8b5fc36e4d9c9adb", null ]
];