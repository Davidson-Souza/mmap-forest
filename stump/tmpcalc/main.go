package main

import (
    "encoding/hex"
    "fmt"

    "github.com/utreexo/utreexod/chaincfg/chainhash"
    "github.com/utreexo/utreexod/wire"
)

func mustHash(str string) chainhash.Hash {
    h, err := chainhash.NewHashFromStr(str)
    if err != nil {
        panic(err)
    }
    return *h
}

func mustHex(str string) []byte {
    b, err := hex.DecodeString(str)
    if err != nil {
        panic(err)
    }
    return b
}

func main() {
    ld := wire.LeafData{
        BlockHash: mustHash("00000032bb881de703dcc968e8258080c7ed4a2933e3a35888fa0b2f75f36029"),
        OutPoint: wire.OutPoint{
            Hash:  mustHash("b38fef50592017cfafbcab88eb3d9cf50b2c801711cad8299495d26df5e54812"),
            Index: 0,
        },
        Amount:     5000000000,
        PkScript:   mustHex("0014fd09839740f0e0b4fc6d5e2527e4022aa9b89dfa"),
        Height:     2,
        IsCoinBase: true,
    }

    hash := ld.LeafHash()
    fmt.Printf("%x\n", hash)
}
