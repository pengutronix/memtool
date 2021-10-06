memtool
=======

memtool is a program that allows to access memory mapped registers. This is
useful to inspect and modify registers from the command line.

By default memtool uses `/dev/mem` to operate. So to actually work you need to
have `DEVMEM` enabled in the kernel. Note that depending on further kernel
configuration you can only access unused io-memory, see the kernel
configuration knobs `IO_STRICT_DEVMEM` and `STRICT_DEVMEM`. Also note that
there might be further restrictions. E.g. on several i.MX ARM SoCs register
access is not possible from user mode for certain areas unless configured
otherwise.

memtool can also operate on plain files, and access PHY registers (via the
`ioctl`s `SIOCSMIIREG` and `SIOCGMIIREG`).

Examples
---------

 * Write a 32-bit wide 0 to register at address 0x73f00040

    ```sh
    # memtool mw -l 0x73f00040 0
    ```

 * Read the first two registers of the default PHY attached to network device
   `eth0`:

    ```sh
    # memtool md -s mdio:eth0. -w 0+4
    ```

 * Place a (usually red) dot on the framebuffer:

    ```sh
    # memtool mw -d /dev/fb0 -w 0 0xfc00
    ```

