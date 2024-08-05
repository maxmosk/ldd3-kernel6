Статьи:

    [1] https://www.kernel.org/doc/html/next/index.html# - полная документация на ядро linux, с некторыми примерами кода

    [2] https://prog.world/linux-kernel-5-0-we-write-simple-block-device-under-blk-mq/ - простое блочное устройство для современных ядер
    [3] https://habr.com/ru/companies/veeam/articles/446148/ - перевод статьи [2] на русский язык

    [4] https://fastbitlab.com/tag/linux-device-driver-programming/ - серия лекций посвященных разработке модулей ядра
    [5] https://linux-kernel-labs.github.io/refs/heads/master/ - серия лекций и лабораторных работ посвященных операционным системам, с примерами написания моудлей ядра

    [6] https://habr.com/ru/companies/yandex/articles/567134/ - Разработка и эксплуатация ядра Linux в инфраструктуре Яндекса

Видео:

    https://www.youtube.com/watch?v=IXBC85SGC0Q&t=1s - запись трансляции написания USB устройства

    https://www.youtube.com/watch?v=XoYkHUnmpQo - небольшая лекция по структуре модулей и символьным устройствам

    https://www.youtube.com/watch?v=s1OcJB3LOV4&list=PLCGpd0Do5-I0LUuFImUlkj2RhoiMCFPhe - серия видеоуроков по написанию USB устройства

    https://www.youtube.com/watch?v=is9wVOKeIjQ - запись трансляции посвященной реверс инжинирингу USB устройства

    https://www.youtube.com/watch?v=NYRhkGrt4Q4&list=PLM8zRjaI08aQKKdUIqObqLTp4o5A67pOy - серия относительно простых уроков по написанию модулей ядра, рассматриваются символьные и USB устройства

    https://www.youtube.com/watch?v=HbQ6q3skZgw - лекция про принцип работы протокола USB

    https://www.youtube.com/watch?v=wB5PAFnk6L4&list=PLc7W4b0WHTAX4F1Byvs4Bp7c8yCDSiKa9&index=3 - небольшая серия уроков посвященная написанию GPIO модуля для Raspberry Pi

    https://www.youtube.com/@LinuxKernelFoundation/featured - канал посвященный разработке модулей ядра

    https://www.youtube.com/watch?v=Zn8mEZXr6nE&list=PL2GL6HVUQAuksbptmKC7X4zruZlIl59is - серия простых видеоуроков посвященная написанию символьных устройств

Примеры кода на данную тематику:

    https://github.com/sysprog21/simplefs - исходный код модуля простой файловой системы
    https://github.com/sysprog21/concurrent-programs - исходный код для курса Linux Kernel Internals

    https://github.com/sysprog21/cirbuf - циклический буффер, может быть полезным при написании своих модулей ядра

    Несколько интересных проектов связанных с написанием модулей ядра:
        https://github.com/sysprog21/ksort
        https://github.com/sysprog21/sched-plugin
        https://github.com/sysprog21/dont-trace
        ps: в принципе в https://github.com/sysprog21 есть множество интересных проектов, которым можно уделить время.
        https://github.com/dicksites/KUtrace