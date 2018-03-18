# Impromptu Modular: Modules for VCV Rack by Marc Boulé

## License

Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt and graphics from the Component Library by Wes Milholen.

See ./LICENSE.txt for all licenses.

See ./res/fonts/ for font licenses.

## Releases

TODO: See [Release Page](https://github.com/MarcBoule/ImpromptuModular/releases) for Mac, Win and Linux builds


# Modules

## Write-Seq-32

![IM](WriteSeq32.jpg)

Three channel 32-step writable sequencer module. This sequencer was designed to allow the entering of notes into a sequencer in a quick and natural manner when a midi keyboard is connected to Rack. A soft keyboard such as VMPK ([Virtual Midi Piano Keyboard](http://vmpk.sourceforge.net/)) or the Autodafe keyboard in Rack can also be used. Although the display shows note names (ex. C4#, D5, etc.), any voltage within the -10V to 10V range can be stored/played in the sequencer, whether it is used as a pitch CV or not.

Ideas: if you have a sustain pedal, use Rack's Midi-CC module and connect the footpedal to the WRITE input to coordinate your writing of sequence notes while playing the keyboard. Turn on AUTOSTEP in Write-Seq-32 to step the sequencer forward on each write. Optionally, if you have a three-pedal foot controller, use the other pedals with the STEP L and STEP R inputs to control the sequencer position manually. Or, more simply, instead of sending the midi keyboard's gate signal into Write-Seq-32's gate input, send it to the write signal, and with autostep, each key-press will automatically be entered in sequence.

Here are some specific details on each element on the faceplate of the module.

* **Autostep**: Will automatically step the sequencer one step right on each write. No effect on channels 1 to 3 when the sequencer is running.

* **Window**: LED buttons to display the active 8-step window from the 32 step sequence (hence four windows). No effect on channels 1 to 3 when the sequencer is running.

* **Sharp / flat**: determines whether to display notes corresponding to black keys using either the sharp or flat symbols used in music notation. See _Notes display_ below for more information.

* **Quantize**: Quantizes the CV IN input to a regular 12 semi-tone equal temperament scale. Since this quantizes the CV IN, some channels can have quantized pitches while others do not. 

* **Step LEDs**: Shows the current position of the sequencer in the given window.

* **Notes display**: Shows the note names for the 8 steps corresponding to the active window. When a stored pitch CV has not been quantized, the display shows the closest such note name. For example, 0.03 Volts is shown as C4, whereas 0.05 V is shown as C4 sharp or D4 flat. Octaves above 9 or below 0 are shown with a top bar and an underscore bar respectively.

* **Gates**: Shows the gate enables for the 8 steps in the current window. See Gate 1-3 below for more information on gate signals. Gates can be toggled whether the sequencer is running or not.

* **Chan**: Selects the channel that is to be displayed/edited in the top part of the module. Even though this is a three channel sequencer, a fourth channel is available for staging a sequence while the sequencer is running. 

* **C & P**: Copy and paste the CVs and gates of a channel into another channel. In a given channel, press the left button (C) to copy the channel into a buffer, then select another channel and press the right button (P) to paste. All 32 steps are copied irrespective of the STPES knob setting.

* **Paste sync**: Determines whether to paste in RealTime, on the next clock (CLK), or at the next sequence start (SEQ). Pending pastes are cleared when the RUNNING 1-3 button is toggled.

* **Step L/R**: Steps the sequencer one step left or right. No effect on channels 1 to 3 when the sequencer is running.

* **Run 1-3**: When running, the sequencer responds to rising edges of the CLOCK input and will step all channels except the staging area (channel 4).

* **Write**: This writes the pitch CV given in the CV IN input into the CV of the current step of the selected channel. If a wire is connected to the GATE IN input, this gate input is also written into the gate enable of the current step/channel. An enabled gate corresponds to a voltage of 1.0V or higher. No effect on channels 1 to 3 when the sequencer is running.

* **CV In**: This pitch CV is written into the current step of the selected channel. Any voltage between -10.0 and 10.0 is supported. See _Notes display_ and _Quantize_ above for more related information. No effect on channels 1 to 3 when the sequencer is running.

* **Gate In**: Allows the state of the gate of the current step/channel to be written. If no wire is connected, input is ignored and the currently stored gate is unaffected. No effect on channels 1 to 3 when the sequencer is running.

* **Steps**: Sets the number of steps in all the sequences (sequence length). Since all channels are synchronized to the same clock, this applies to all sequences (i.e. sequences are all of the same length).

* **Monitor**: this switch determines which pitch CV will be routed to the currently selected channel's CV output. When the switch is in the left position, the pitch CV stored in the sequencer at that step is output, whereas in the right position the pitch CV applied to the CV IN jack is output. Has no effect when the sequencer is running.

* **CV 1-3**: pitch CV outputs of each channel.

* **Gate 1-3**: Gate signal outputs for each channel at the current step. The duration of the gates corresponds to the high time of the clock signal.

* **Chan input**: control voltage for channel selection (CHAN button). A rising edge triggered at 1.0V will increment the channel selection by one.

* **Write input**: control voltage for writing CVs into the sequencer (WRITE button). A rising edge triggered at 1.0V will perform the write action (see _Write_ above).

* **Step L/R inputs**: control voltages for step selection (STEP L/R buttons). A rising edge triggered at 1.0V will step the sequencer left/right by one step.

* **Reset input**: repositions the sequencer at the first step. A rising edge triggered at 1.0V will be detected as a reset.

* **Clock**: when the sequencer is running, each rising edge (1.0V threshold) will advance the sequencer by one step. The width (duration) of the high pulse of the clock is used as the width (duration) of the gate outputs. 



## Write-Seq-64

![IM](WriteSeq64.jpg)

Four channel 64-step writable sequencer module. This sequencer is based on Write-Seq-32, both of which share many of the same functionalities. Write-Seq-64 has dual clock inputs (each controls a pair of channels) and allows each channel to have their separate lengths.

Here are some specific details on each element on the faceplate of the module, with emphasis on the differences compared to Write-Seq-32.

* **Chan**: Four channels available, with a fifth channel that can be used as a staging area.

* **Gate/CV**: Gate state and CV of the currently selected step.

* **Steps**: Sets the number of steps of the currently selected sequence (sequence length).

* **Clock 1,2**: Clock signal for channels 1 and 2, and also for channels 3 and 4 when no input is connected to _Clock 3,4_.

* **Reset**: Rests all channels' step positions to the start of their sequences.
