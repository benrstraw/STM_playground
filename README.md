Only the STM32CubeMX project and the relevant source files are included. 

To prepare the rest of the project, open the STM_playground.ioc file in STM32CubeMX and "Generate Code." This will create necessary `startup` and `Drivers` directories and generate an Atollic TrueSTUDIO project file.

So far the only real work that has been done has been familiarizing myself with the STM32CubeMX workflow, how to set up different pins and peripherals, as well as getting timers and interrupts working. Most of the work so far is contained within [the `main()` function of `Src/main.c`](Src/main.c#L102-L112) and [the interrupt handlers in `Src/stm32l1xx_it.c`](Src/stm32l1xx_it.c#L195-L235). The code required to get this working properly was actually pretty simple, the most difficult part by far was learning all the new concepts associated with timers and interrupts, and figuring out how to apply those and get everything set up within the STM32CubeMX configuration tool.
