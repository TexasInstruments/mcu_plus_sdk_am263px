var group__DRV__I2C__HLD__MODULE =
[
    [ "I2C_HwAttrs", "structI2C__HwAttrs.html", [
      [ "baseAddr", "structI2C__HwAttrs.html#a170785636c4238bef72b0092b9316d13", null ],
      [ "intNum", "structI2C__HwAttrs.html#a26e51dabcfc03d1d3f322d9b31adb892", null ],
      [ "eventId", "structI2C__HwAttrs.html#ad42e910323c9064afdb343cd54774e5c", null ],
      [ "funcClk", "structI2C__HwAttrs.html#ae400990b8899a899af56b671a7a98b9f", null ],
      [ "enableIntr", "structI2C__HwAttrs.html#a9b5cd4f1e9a11365b4fbde7b3409c9ee", null ],
      [ "ownTargetAddr", "structI2C__HwAttrs.html#af151f33358d75a3425952786e76e214d", null ]
    ] ],
    [ "I2C_Mem_Transaction", "structI2C__Mem__Transaction.html", [
      [ "memAddr", "structI2C__Mem__Transaction.html#af2454cf7119bd9a966577cb2145a79a9", null ],
      [ "memAddrSize", "structI2C__Mem__Transaction.html#ae221f896d08b063919e461d4ba752574", null ],
      [ "buffer", "structI2C__Mem__Transaction.html#a93ca12ab573bf98cd3400714b2be50d2", null ],
      [ "size", "structI2C__Mem__Transaction.html#a6ee1e80f3a00fc03e6b947a7da35fa80", null ],
      [ "memDataDir", "structI2C__Mem__Transaction.html#abffc3b346ffc221d4424b72a972dec24", null ]
    ] ],
    [ "I2C_Transaction", "structI2C__Transaction.html", [
      [ "writeBuf", "structI2C__Transaction.html#a3a80191f72d53c793b58c17cf2361da6", null ],
      [ "writeCount", "structI2C__Transaction.html#a08959a9f098163d02d6002e73b3a4606", null ],
      [ "readBuf", "structI2C__Transaction.html#a804b918d5cb08bb75a39866d0b89e01e", null ],
      [ "readCount", "structI2C__Transaction.html#a2085d4c943f0d8617365b7c427e6a0de", null ],
      [ "targetAddress", "structI2C__Transaction.html#a56940dfa9a256cfd16144d4e61d64188", null ],
      [ "nextPtr", "structI2C__Transaction.html#a21755edd4d999d6041d303e269596371", null ],
      [ "arg", "structI2C__Transaction.html#a29ec65addac2d4ef5d1235c2329e2fc1", null ],
      [ "timeout", "structI2C__Transaction.html#ad5959435aa2266d8b9af752b1a2e14c6", null ],
      [ "controllerMode", "structI2C__Transaction.html#ae2e46ac2e1fe354494ebc47fbd131ea3", null ],
      [ "expandSA", "structI2C__Transaction.html#ab3fba2e46341efc0ff19e21a13194b17", null ],
      [ "memTxnEnable", "structI2C__Transaction.html#afa4e6dc5a33d862aa60f15383e6d0e17", null ],
      [ "memTransaction", "structI2C__Transaction.html#afa33691bf2059e699e0813867bc228c4", null ],
      [ "status", "structI2C__Transaction.html#a343a6e4d485f58b098eeb167c76ccac7", null ]
    ] ],
    [ "I2C_Params", "structI2C__Params.html", [
      [ "transferMode", "structI2C__Params.html#ab72ce0fd173d102656a5b482ad6f3779", null ],
      [ "transferCallbackFxn", "structI2C__Params.html#a56e74ffc2f1ee70f715dcb41874f153c", null ],
      [ "bitRate", "structI2C__Params.html#a7f9942a2c6bab07474666638d0dfdc57", null ]
    ] ],
    [ "I2C_Object", "structI2C__Object.html", [
      [ "mutex", "structI2C__Object.html#a09cad8965bfd3b85fe50ea822580eda7", null ],
      [ "transferComplete", "structI2C__Object.html#ada53053790f68feab247d1452cb646ab", null ],
      [ "hwiObj", "structI2C__Object.html#a76cc5a10bfabe817395d18b7de6e3202", null ],
      [ "i2cParams", "structI2C__Object.html#a91311572f00321c4e09f9c7211b188d1", null ],
      [ "state", "structI2C__Object.html#a21aab1e2c9725ec2f059458ad263d992", null ],
      [ "currentTransaction", "structI2C__Object.html#ab08fd2d0ee8a59fe0a39dc636e2e50b5", null ],
      [ "headPtr", "structI2C__Object.html#a926e5faed22a7e4d797a417301dd0d2f", null ],
      [ "tailPtr", "structI2C__Object.html#a9c5bac93e68090c87fafc284eff18e79", null ],
      [ "isOpen", "structI2C__Object.html#a00a4f93fca8c1fda9e24343353b9bff9", null ],
      [ "intStatusErr", "structI2C__Object.html#ad6a25967b42125c9b81c9bd8a600b117", null ],
      [ "i2cLldObject", "structI2C__Object.html#a7923b94eff66662bf2e087943360ae89", null ],
      [ "i2cLldHandle", "structI2C__Object.html#aff386dafe5a558b55af8c556841ca4de", null ]
    ] ],
    [ "I2C_Config", "structI2C__Config.html", [
      [ "object", "structI2C__Config.html#a808ece57f43ac1590894a78da5339588", null ],
      [ "hwAttrs", "structI2C__Config.html#a11198cd39cfaafd437f3ae5eeaf2f98f", null ]
    ] ],
    [ "I2C_MODE_BLOCKING", "group__DRV__I2C__HLD__MODULE.html#ga9c68ee5361da5c32b6f588106c5b9a1b", null ],
    [ "I2C_MODE_CALLBACK", "group__DRV__I2C__HLD__MODULE.html#gadd13e093bc1d2316a49225f98503da43", null ],
    [ "I2C_MEM_TXN_DIR_INVALID", "group__DRV__I2C__HLD__MODULE.html#ga91161f82e72a0d0d1a564abd16589286", null ],
    [ "I2C_MEM_TXN_DIR_TX", "group__DRV__I2C__HLD__MODULE.html#gafe22f70145aaecfcb33d7f574bd51756", null ],
    [ "I2C_MEM_TXN_DIR_RX", "group__DRV__I2C__HLD__MODULE.html#ga41c48490edc1d4ddec6e295f696d50ee", null ],
    [ "I2C_Handle", "group__DRV__I2C__HLD__MODULE.html#gadd75c22230842b00010efe1d992b8979", null ],
    [ "I2C_CallbackFxn", "group__DRV__I2C__HLD__MODULE.html#ga7fb6ee324a498567ae7302989bf68175", null ],
    [ "I2C_init", "group__DRV__I2C__HLD__MODULE.html#ga9ff51ddf1d325776fef90cce0223772b", null ],
    [ "I2C_deinit", "group__DRV__I2C__HLD__MODULE.html#ga4d7f9753204b229788a8a1c34585b0d0", null ],
    [ "I2C_Params_init", "group__DRV__I2C__HLD__MODULE.html#gab11636302074d67180207ab81ceb323c", null ],
    [ "I2C_open", "group__DRV__I2C__HLD__MODULE.html#ga942a6589f060e6a9becb4eae13eee8f5", null ],
    [ "I2C_close", "group__DRV__I2C__HLD__MODULE.html#ga12c86d89a687f2ee1eb980d99c32326d", null ],
    [ "I2C_Memory_Transaction_init", "group__DRV__I2C__HLD__MODULE.html#gaa0ce3ff0ccb835a777c7bc230751608b", null ],
    [ "I2C_Transaction_init", "group__DRV__I2C__HLD__MODULE.html#ga533741f4b49d58b75b39053643ac9b64", null ],
    [ "I2C_transfer", "group__DRV__I2C__HLD__MODULE.html#ga6c82327e0d17600c67b45a8616905c10", null ],
    [ "I2C_memory_transfer", "group__DRV__I2C__HLD__MODULE.html#gafc60312fcfdb1b14d3e7206cadebdaf1", null ],
    [ "I2C_probe", "group__DRV__I2C__HLD__MODULE.html#ga1d4f987df1308ed8dc7bcc70e61f5711", null ],
    [ "I2C_setBusFrequency", "group__DRV__I2C__HLD__MODULE.html#gad78db5aecccd790b4e9b5b45cbb05f5b", null ],
    [ "I2C_getHandle", "group__DRV__I2C__HLD__MODULE.html#gae72fbbef5977e287d18c7fb842464c6e", null ]
];