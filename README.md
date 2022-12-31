PWM Player
==========

(English description is [below](#eng))  

PWM Player - это игралка простейших мелодий для систем, оснащённых пищалкой, подключенной к программно управляемому широтно-импульсному модулятору (он же ШИМ, он же PWM - pulse-width modulator).
ШИМ должен быть доступен в линуксе как устройство `/sys/class/pwm/pwmchip0/pwm<N>`, где N - номер ШИМ устройства.
Возможности такого звукового устройства недостаточны для полноценного воспроизведения музыки, поэтому программа воспроизводит только простейшие мелодии, заданные в форматах eMelody/iMelody (форматы рингтонов для кнопочных телефонов, обычно это файлы с расширением .emy/.imy) и небольшое подмножество MIDI-файлов (раширение .mid)

Простейший вариант использования - проиграть мелодию из файла:  
`pwm-player -m melody.mid`   
`pwm-player -i melody.imy`  
`pwm-player -e melody.emy`  

Если в вашей системе не определена переменная окружения `WB_PWM_BUZZER`, задающая номер PWM устройства для проигрывания, этот номер нужно задать ключём `-p`  
`pwm-player -p 2 -m melody.mid`   

Если вы хотите пропищщать не весь MIDI-файл, а только его часть, можно задать ограничения ключами `-t` (задаёт номер MIDI-трека для проигрывания, нумерация начинается с нуля) и `-n` (задаёт номера последовательных нот для проигрывания)  
`pwm-player -t 1 -n 10:20 -m melody.mid`  
будет играть ноты с 10-ой по 20-ю (включительно) первого трека.  

Мелодии в формате eMelody/iMelody можно задавать прямо в командной строке  
`pwm-player -I "*5b3g3b2b3#f3b2b3*4b3*5#d3#f3b2*4b1b1;"`  
`pwm-player -E +C+G+Epppp+C+G`  

Если звук плохо слышно, можно попробовать увеличить громкость (в примере - 120% к номиналу, однако диапазон регулировки на самом деле довольно скромный, из-за особенностей работы ШИМ)  
`pwm-player -v 120 -m melody.mid`   

Ключ `-b` запускает проигрывание мелодии в фоновом режиме, а `-d` включает отладочную печать, изучив которую можно попытаться понять, почему оно не работает (так как надо)...

  
  
  
  
---  
  
  
  


<a id="eng"/>
**PWM Player** plays simple melodies using buzzer attached to a software-controlled PWM device, that should be available in linux under `/sys/class/pwm/pwmchip0/pwm<N>`, where N is PWM device number.  
It does not meant to be a music player due to a poor performance of PWM controlled audio output. Yet it is quite sufficient to produce audio notifications for headless linux boxes from iMelody/eMelody or MIDI sources.

In order to play a melody file just specify it with appropriate option:  
`pwm-player -m melody.mid`   
`pwm-player -i melody.imy`  
`pwm-player -e melody.emy`  

Player takes PWM device number from the environment variable `WB_PWM_BUZZER` by default, yet you may specify/override it using `-p` command line option.  
`pwm-player -p 2 -m melody.mid`   

You may limit MIDI melody to a certain sequence of notes (option `-n`) and/or select MIDI track to be played with option `-t`  
`pwm-player -t 1 -n 10:20 -m melody.mid`  
will play notes from 10th to 20th (inclusive) from a first MIDI track.

Also it is possible to provive eMelody/iMelody directly using options `-E` and `-I` respectively  
`pwm-player -I "*5b3g3b2b3#f3b2b3*4b3*5#d3#f3b2*4b1b1;"`  
`pwm-player -E +C+G+Epppp+C+G`  

To amend playing volume use option `-v` (percentage to the default value, i.e. 100 means to use original volume level)  
`pwm-player -v 120 -m melody.mid`   

To play melody in background use option `-b`, and to get some debug output - option `-d`



Would you find this program useful and wish to thank the author and to encourage his further creativity, your donations will be gratefully accepted within the following wallets:  
bitcoin:18TrRxz3kY853AL8PvB12aASmzWoLNTHkF  
litecoin:ltc1qrv33sp3adalx9lueg8nxm5gj7ptndceqkt02kv  
monero:84zqtyCWWM37dxN1HuZV5Ebbx8Zn61DcRQwazpjtieAPSQ9jHJtghq1ew4qKsNoapdGKzXVtz3TgxJkfKpZUyiJHHZRWG1a  

Original location: https://github.com/sthamster/pwm-player  

