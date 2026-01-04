Como se menciona na wiki, o desenvolvemento do buzón dividiuse en 4 placas. Neste directorio móstrase a implementación da Placa 3, que inclúe os sensores de IR, presión e colisión superior do buzón.

En canto ao funcionamento, cando se presiona o sensor de colisión (é dicir, cando se abre a porta superior do buzón), iníciase a lectura do sensor IR, que detecta a introdución de cartas. O sensor de presión permanece sempre activo á espera de cambios.

En Node‑RED publícanse os seguintes tópicos:

- buzon/ir: o timestamp cada vez que se introduce unha carta. Úsase para mostrar a hora e data da última carta introducida e para realizar o reconto.

- buzon/presion: un JSON co valor da presión e o valor máximo que pode ler o sensor. Úsase para indicar se o buzón está baleiro (0), se contén algo (1– máx‑1) ou se está cheo (valor == límite).

- buzon/colision/superior: 0 ou 1 segundo se a porta está pechada ou aberta, respectivamente. Úsase para mostrar o estado da porta superior.

>As mentioned in the wiki, the development of the mailbox has been divided into 4 boards. This directory shows the >implementation of Board 3, which includes the IR, pressure, and upper collision sensors of the mailbox.
>
>Talking about its behaviour, when the collision sensor is pressed (when the upper door of the mailbox is opened), the IR sensor begins reading and detects the insertion of letters. The pressure sensor remains active at all times, waiting for changes.

>In Node‑RED, the following topics are published:
>
>buzon/ir: the timestamp each time a letter is inserted. It is used to display the date and time of the last inserted letter and >to perform the counting.
>
>buzon/presion: a JSON containing the pressure value and the maximum value the sensor can read. It is used to indicate whether >the mailbox is empty (0), contains something (1–max‑1), or is full (value == limit).
>
>buzon/colision/superior: 0 or 1 depending on whether the door is closed or open, respectively. It is used to display the state >of the upper door.