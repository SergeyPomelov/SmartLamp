// Если вы хотите добавить эффекты или сделать им копии для демонстрации на разных настройках, нужно делать это в 4 местах (помимо добавления программы самого эффекта в effects.ino):
// 1. в файле Constants.h - придумываются названия задаются и порядковые эффектам. Не изменяйте настройки у эффектов с 7U по 15U! В конце указывается общее количество MODE_AMOUNT.
// 2. там же в файле Constants.h - задаётся Реестр эффектов для передачи в приложение. Он живёт отдельно. Если у вас приложение не поддерживает запрос реестра у лампы, его можно не менять.
// 3. в файле GyverLamp_v1.5.ino - стоит указать написать количество ноликов по числу эффектов в строчке uint8_t FavoritesManager::FavoriteModes[MODE_AMOUNT] = {0, 0, 0, 0, 0, 0, 0, ...
// 4. в файле effectTicker.ino - подключается процедура вызова эффекта на соответствующий ей EFF_...

uint32_t effTimer;

void effectsTick()
{
  if (!dawnFlag)
  {
    if (ONflag && (millis() - effTimer >= ((currentMode < 7 || currentMode > 15) ? 256U - modes[currentMode].Speed : 50)))
    {
      effTimer = millis();
      switch (currentMode)
      {
        case EFF_SPARKLES:       sparklesRoutine();           break;
        case EFF_FIRE:           fireRoutine(true);           break;
        //        case EFF_WHITTE_FIRE:    fireRoutine(false);          break;
        case EFF_WHITTE_FIRE:    fire2012WithPalette();       break;      // <- изменили белый огонь на Водопад
        case EFF_RAINBOW_VER:    rainbowVerticalRoutine();    break;
        case EFF_RAINBOW_HOR:    rainbowHorizontalRoutine();  break;
        case EFF_RAINBOW_DIAG:   rainbowDiagonalRoutine();    break;
        case EFF_COLORS:         colorsRoutine();             break;
        case EFF_MADNESS:        madnessNoiseRoutine();       break;
        case EFF_CLOUDS:         cloudsNoiseRoutine();        break;
        case EFF_LAVA:           lavaNoiseRoutine();          break;
        case EFF_PLASMA:         plasmaNoiseRoutine();        break;
        case EFF_RAINBOW:        rainbowNoiseRoutine();       break;
        case EFF_RAINBOW_STRIPE: rainbowStripeNoiseRoutine(); break;
        case EFF_ZEBRA:          zebraNoiseRoutine();         break;
        case EFF_FOREST:         forestNoiseRoutine();        break;
        case EFF_OCEAN:          oceanNoiseRoutine();         break;
        case EFF_COLOR:          colorRoutine();              break;
        case EFF_SNOW:           snowRoutine();               break;
        case EFF_SNOWSTORM:      snowStormRoutine();          break;
        case EFF_STARFALL:       starfallRoutine();           break;
        case EFF_MATRIX:         matrixRoutine();             break;
        case EFF_LIGHTERS:       lightersRoutine();           break;
        //        case EFF_LIGHTER_TRACES: ballsRoutine();              break;
        case EFF_LIGHTER_TRACES: RainbowCometRoutine();       break;      // <- изменили Светлячки со шлейфом на Кометы
        //        case EFF_LIGHTER_TRACES: NoiseStreamingRoutine();     break;      // <- изменили Светлячки со шлейфом на Кометы мини
        case EFF_PAINTBALL:      lightBallsRoutine();         break;
        case EFF_CUBE:           ballRoutine();               break;
        case EFF_WHITE_COLOR:    whiteColorStripeRoutine();   break;
        default:                                              break;
      }
      FastLED.show();
    }
  }
}

void changePower()
{
  if (ONflag)
  {
    effectsTick();
    for (uint8_t i = 0U; i < modes[currentMode].Brightness; i = constrain(i + 8, 0, modes[currentMode].Brightness))
    {
      FastLED.setBrightness(i);
      delay(1);
      FastLED.show();
    }
    FastLED.setBrightness(modes[currentMode].Brightness);
    delay(2);
    FastLED.show();
  }
  else
  {
    effectsTick();
    for (uint8_t i = modes[currentMode].Brightness; i > 0; i = constrain(i - 8, 0, modes[currentMode].Brightness))
    {
      FastLED.setBrightness(i);
      delay(1);
      FastLED.show();
    }
    FastLED.clear();
    delay(2);
    FastLED.show();
  }

  TimerManager::TimerRunning = false;
  TimerManager::TimerHasFired = false;
  TimerManager::TimeToFire = 0ULL;

  if (FavoritesManager::UseSavedFavoritesRunning == 0U)     // если выбрана опция Сохранять состояние (вкл/выкл) "избранного", то ни выключение модуля, ни выключение матрицы не сбрасывают текущее состояние (вкл/выкл) "избранного"
  {
    FavoritesManager::TurnFavoritesOff();
  }
}
