package main

import (
    "fmt"
    "math/rand"
    "time"
)

func hash(seg, factor int) {
    // 随机种子
    const asize = 1 << 21
    //const seg   = 2
    var dsize = asize * factor / 100

    fmt.Println("size = ", asize, "seg = ", seg, "load = ", 100 * dsize / asize)

    rand.Seed(time.Now().Unix())
    var arr = make ([]int, (asize + seg - 1) / seg)

    for i := 0; i < dsize; i++ {
        arr[rand.Intn(asize) / seg] += 1
    }

    var sarr = make ([]int, len(arr))
    for i := 0; i < len(arr); i++ {
        sarr[arr[i]] += 1
    }

    var sum = 0
    for i := 0; i < 1000; i++ {
        sum += sarr[i]
        fmt.Println(i, sarr[i],  sarr[i] * 1000 / len(arr), "/1000",  (1000.0 * sum) / len(arr))
        if sum == len(arr) {
           break
        }
    }
}

func main() {
    for i := 16; i > 0; i /= 2 {
        hash(i, 90)
    }

    for f := 90; f > 30; f -= 10 {
        hash(16, f)
    }
}
