# Guitarra iRig — app nativo (baixa latência)

App Android gratuito e de código aberto para tocar guitarra pelo iRig
com canal **limpo** e **overdrive**, usando a biblioteca Oboe do Google
(AAudio em modo exclusivo) para latência mínima.

## Como compilar

1. Instale o **Android Studio** (https://developer.android.com/studio)
2. Abra este projeto: **File > Open** e selecione a pasta `GuitarFX`
3. Aguarde a sincronização do Gradle (na primeira vez ele baixa o SDK,
   o **NDK** e o **CMake** automaticamente — se pedir para instalar
   "NDK" ou "CMake", aceite)
4. **Build > Build App Bundle(s) / APK(s) > Build APK(s)**
5. O APK sai em `app/build/outputs/apk/debug/app-debug.apk`

## Como usar

1. Guitarra -> iRig -> entrada P2 do celular; fone na saída do iRig
2. Abra o app, toque em LIGAR e permita o microfone
3. LIMPO / OVERDRIVE alternam o canal; Drive e Tom só agem no overdrive

## Distribuição

O APK pode ser distribuído livremente (link direto, site, F-Droid).
Licença sugerida: MIT — use, copie e modifique à vontade.

## Apoie o projeto (doação via PIX)
Este app é gratuito e de código aberto. Se ele foi útil pra você, considere uma doação via PIX:

**Chave aleatória:** `fce13f3c-310d-4c5a-8816-efb2e0458842`
**Beneficiário:** Claudio Jose Toldo — Criciúma/SC

![QR Code PIX](pix_qrcode.png)

Ou copie o código abaixo e cole no Pix Copia e Cola do app do seu banco:

```
00020101021126580014br.gov.bcb.pix0136fce13f3c-310d-4c5a-8816-efb2e04588425204000053039865802BR5918CLAUDIO JOSE TOLDO6008CRICIUMA62070503***6304BDB9
```
