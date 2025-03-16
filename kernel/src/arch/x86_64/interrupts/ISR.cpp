/*
Copyright (©) 2024-2025  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ISR.hpp"
#include "IDT.hpp"

#include "../Panic.hpp"

const char* g_Exceptions[32] = {
    "Divide by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void x86_64_ISR_SetIDTHandlers();

x86_64_ISRHandler_t g_ISR_Handlers[256];

void x86_64_Convert_ISRRegs_To_StandardRegs(x86_64_ISR_Frame* frame, x86_64_Registers* state) {
    if (state == nullptr || frame == nullptr)
        return;
    state->RAX = frame->RAX;
    state->RBX = frame->RBX;
    state->RCX = frame->RCX;
    state->RDX = frame->RDX;
    state->RSI = frame->RSI;
    state->RDI = frame->RDI;
    state->RSP = frame->RSP;
    state->RBP = frame->RBP;
    state->R8 = frame->R8;
    state->R9 = frame->R9;
    state->R10 = frame->R10;
    state->R11 = frame->R11;
    state->R12 = frame->R12;
    state->R13 = frame->R13;
    state->R14 = frame->R14;
    state->R15 = frame->R15;
    state->RIP = frame->RIP;
    state->CS = (uint16_t)frame->CS;
    state->SS = (uint16_t)frame->SS;
    state->RFLAGS = frame->RFLAGS;
    state->CR3 = frame->CR3;
    state->_align = 0;
}

void x86_64_InitISRs() {
    for (int i = 0; i < 256; i++)
        g_ISR_Handlers[i] = nullptr;

    x86_64_ISR_SetIDTHandlers();
}

void x86_64_ISR_RegisterHandler(uint8_t vector, x86_64_ISRHandler_t handler) {
    g_ISR_Handlers[vector] = handler;
}

bool in_exception = false;

extern "C" void x86_64_ISR_Handler(x86_64_ISR_Frame* frame) {
    if (g_ISR_Handlers[frame->INT] != nullptr)
        return g_ISR_Handlers[frame->INT](frame);

    if (in_exception) {
        while (true) {
            __asm__ volatile ("hlt");
        }
    }

    in_exception = true;

    char const* reason = "Unhandled interrupt";
    if (frame->INT < 32)
        reason = g_Exceptions[frame->INT];

    x86_64_Panic(reason, frame, true);
}

extern "C" {

extern void x86_64_ISR_Handler0();
extern void x86_64_ISR_Handler1();
extern void x86_64_ISR_Handler2();
extern void x86_64_ISR_Handler3();
extern void x86_64_ISR_Handler4();
extern void x86_64_ISR_Handler5();
extern void x86_64_ISR_Handler6();
extern void x86_64_ISR_Handler7();
extern void x86_64_ISR_Handler8();
extern void x86_64_ISR_Handler9();
extern void x86_64_ISR_Handler10();
extern void x86_64_ISR_Handler11();
extern void x86_64_ISR_Handler12();
extern void x86_64_ISR_Handler13();
extern void x86_64_ISR_Handler14();
extern void x86_64_ISR_Handler15();
extern void x86_64_ISR_Handler16();
extern void x86_64_ISR_Handler17();
extern void x86_64_ISR_Handler18();
extern void x86_64_ISR_Handler19();
extern void x86_64_ISR_Handler20();
extern void x86_64_ISR_Handler21();
extern void x86_64_ISR_Handler22();
extern void x86_64_ISR_Handler23();
extern void x86_64_ISR_Handler24();
extern void x86_64_ISR_Handler25();
extern void x86_64_ISR_Handler26();
extern void x86_64_ISR_Handler27();
extern void x86_64_ISR_Handler28();
extern void x86_64_ISR_Handler29();
extern void x86_64_ISR_Handler30();
extern void x86_64_ISR_Handler31();
extern void x86_64_ISR_Handler32();
extern void x86_64_ISR_Handler33();
extern void x86_64_ISR_Handler34();
extern void x86_64_ISR_Handler35();
extern void x86_64_ISR_Handler36();
extern void x86_64_ISR_Handler37();
extern void x86_64_ISR_Handler38();
extern void x86_64_ISR_Handler39();
extern void x86_64_ISR_Handler40();
extern void x86_64_ISR_Handler41();
extern void x86_64_ISR_Handler42();
extern void x86_64_ISR_Handler43();
extern void x86_64_ISR_Handler44();
extern void x86_64_ISR_Handler45();
extern void x86_64_ISR_Handler46();
extern void x86_64_ISR_Handler47();
extern void x86_64_ISR_Handler48();
extern void x86_64_ISR_Handler49();
extern void x86_64_ISR_Handler50();
extern void x86_64_ISR_Handler51();
extern void x86_64_ISR_Handler52();
extern void x86_64_ISR_Handler53();
extern void x86_64_ISR_Handler54();
extern void x86_64_ISR_Handler55();
extern void x86_64_ISR_Handler56();
extern void x86_64_ISR_Handler57();
extern void x86_64_ISR_Handler58();
extern void x86_64_ISR_Handler59();
extern void x86_64_ISR_Handler60();
extern void x86_64_ISR_Handler61();
extern void x86_64_ISR_Handler62();
extern void x86_64_ISR_Handler63();
extern void x86_64_ISR_Handler64();
extern void x86_64_ISR_Handler65();
extern void x86_64_ISR_Handler66();
extern void x86_64_ISR_Handler67();
extern void x86_64_ISR_Handler68();
extern void x86_64_ISR_Handler69();
extern void x86_64_ISR_Handler70();
extern void x86_64_ISR_Handler71();
extern void x86_64_ISR_Handler72();
extern void x86_64_ISR_Handler73();
extern void x86_64_ISR_Handler74();
extern void x86_64_ISR_Handler75();
extern void x86_64_ISR_Handler76();
extern void x86_64_ISR_Handler77();
extern void x86_64_ISR_Handler78();
extern void x86_64_ISR_Handler79();
extern void x86_64_ISR_Handler80();
extern void x86_64_ISR_Handler81();
extern void x86_64_ISR_Handler82();
extern void x86_64_ISR_Handler83();
extern void x86_64_ISR_Handler84();
extern void x86_64_ISR_Handler85();
extern void x86_64_ISR_Handler86();
extern void x86_64_ISR_Handler87();
extern void x86_64_ISR_Handler88();
extern void x86_64_ISR_Handler89();
extern void x86_64_ISR_Handler90();
extern void x86_64_ISR_Handler91();
extern void x86_64_ISR_Handler92();
extern void x86_64_ISR_Handler93();
extern void x86_64_ISR_Handler94();
extern void x86_64_ISR_Handler95();
extern void x86_64_ISR_Handler96();
extern void x86_64_ISR_Handler97();
extern void x86_64_ISR_Handler98();
extern void x86_64_ISR_Handler99();
extern void x86_64_ISR_Handler100();
extern void x86_64_ISR_Handler101();
extern void x86_64_ISR_Handler102();
extern void x86_64_ISR_Handler103();
extern void x86_64_ISR_Handler104();
extern void x86_64_ISR_Handler105();
extern void x86_64_ISR_Handler106();
extern void x86_64_ISR_Handler107();
extern void x86_64_ISR_Handler108();
extern void x86_64_ISR_Handler109();
extern void x86_64_ISR_Handler110();
extern void x86_64_ISR_Handler111();
extern void x86_64_ISR_Handler112();
extern void x86_64_ISR_Handler113();
extern void x86_64_ISR_Handler114();
extern void x86_64_ISR_Handler115();
extern void x86_64_ISR_Handler116();
extern void x86_64_ISR_Handler117();
extern void x86_64_ISR_Handler118();
extern void x86_64_ISR_Handler119();
extern void x86_64_ISR_Handler120();
extern void x86_64_ISR_Handler121();
extern void x86_64_ISR_Handler122();
extern void x86_64_ISR_Handler123();
extern void x86_64_ISR_Handler124();
extern void x86_64_ISR_Handler125();
extern void x86_64_ISR_Handler126();
extern void x86_64_ISR_Handler127();
extern void x86_64_ISR_Handler128();
extern void x86_64_ISR_Handler129();
extern void x86_64_ISR_Handler130();
extern void x86_64_ISR_Handler131();
extern void x86_64_ISR_Handler132();
extern void x86_64_ISR_Handler133();
extern void x86_64_ISR_Handler134();
extern void x86_64_ISR_Handler135();
extern void x86_64_ISR_Handler136();
extern void x86_64_ISR_Handler137();
extern void x86_64_ISR_Handler138();
extern void x86_64_ISR_Handler139();
extern void x86_64_ISR_Handler140();
extern void x86_64_ISR_Handler141();
extern void x86_64_ISR_Handler142();
extern void x86_64_ISR_Handler143();
extern void x86_64_ISR_Handler144();
extern void x86_64_ISR_Handler145();
extern void x86_64_ISR_Handler146();
extern void x86_64_ISR_Handler147();
extern void x86_64_ISR_Handler148();
extern void x86_64_ISR_Handler149();
extern void x86_64_ISR_Handler150();
extern void x86_64_ISR_Handler151();
extern void x86_64_ISR_Handler152();
extern void x86_64_ISR_Handler153();
extern void x86_64_ISR_Handler154();
extern void x86_64_ISR_Handler155();
extern void x86_64_ISR_Handler156();
extern void x86_64_ISR_Handler157();
extern void x86_64_ISR_Handler158();
extern void x86_64_ISR_Handler159();
extern void x86_64_ISR_Handler160();
extern void x86_64_ISR_Handler161();
extern void x86_64_ISR_Handler162();
extern void x86_64_ISR_Handler163();
extern void x86_64_ISR_Handler164();
extern void x86_64_ISR_Handler165();
extern void x86_64_ISR_Handler166();
extern void x86_64_ISR_Handler167();
extern void x86_64_ISR_Handler168();
extern void x86_64_ISR_Handler169();
extern void x86_64_ISR_Handler170();
extern void x86_64_ISR_Handler171();
extern void x86_64_ISR_Handler172();
extern void x86_64_ISR_Handler173();
extern void x86_64_ISR_Handler174();
extern void x86_64_ISR_Handler175();
extern void x86_64_ISR_Handler176();
extern void x86_64_ISR_Handler177();
extern void x86_64_ISR_Handler178();
extern void x86_64_ISR_Handler179();
extern void x86_64_ISR_Handler180();
extern void x86_64_ISR_Handler181();
extern void x86_64_ISR_Handler182();
extern void x86_64_ISR_Handler183();
extern void x86_64_ISR_Handler184();
extern void x86_64_ISR_Handler185();
extern void x86_64_ISR_Handler186();
extern void x86_64_ISR_Handler187();
extern void x86_64_ISR_Handler188();
extern void x86_64_ISR_Handler189();
extern void x86_64_ISR_Handler190();
extern void x86_64_ISR_Handler191();
extern void x86_64_ISR_Handler192();
extern void x86_64_ISR_Handler193();
extern void x86_64_ISR_Handler194();
extern void x86_64_ISR_Handler195();
extern void x86_64_ISR_Handler196();
extern void x86_64_ISR_Handler197();
extern void x86_64_ISR_Handler198();
extern void x86_64_ISR_Handler199();
extern void x86_64_ISR_Handler200();
extern void x86_64_ISR_Handler201();
extern void x86_64_ISR_Handler202();
extern void x86_64_ISR_Handler203();
extern void x86_64_ISR_Handler204();
extern void x86_64_ISR_Handler205();
extern void x86_64_ISR_Handler206();
extern void x86_64_ISR_Handler207();
extern void x86_64_ISR_Handler208();
extern void x86_64_ISR_Handler209();
extern void x86_64_ISR_Handler210();
extern void x86_64_ISR_Handler211();
extern void x86_64_ISR_Handler212();
extern void x86_64_ISR_Handler213();
extern void x86_64_ISR_Handler214();
extern void x86_64_ISR_Handler215();
extern void x86_64_ISR_Handler216();
extern void x86_64_ISR_Handler217();
extern void x86_64_ISR_Handler218();
extern void x86_64_ISR_Handler219();
extern void x86_64_ISR_Handler220();
extern void x86_64_ISR_Handler221();
extern void x86_64_ISR_Handler222();
extern void x86_64_ISR_Handler223();
extern void x86_64_ISR_Handler224();
extern void x86_64_ISR_Handler225();
extern void x86_64_ISR_Handler226();
extern void x86_64_ISR_Handler227();
extern void x86_64_ISR_Handler228();
extern void x86_64_ISR_Handler229();
extern void x86_64_ISR_Handler230();
extern void x86_64_ISR_Handler231();
extern void x86_64_ISR_Handler232();
extern void x86_64_ISR_Handler233();
extern void x86_64_ISR_Handler234();
extern void x86_64_ISR_Handler235();
extern void x86_64_ISR_Handler236();
extern void x86_64_ISR_Handler237();
extern void x86_64_ISR_Handler238();
extern void x86_64_ISR_Handler239();
extern void x86_64_ISR_Handler240();
extern void x86_64_ISR_Handler241();
extern void x86_64_ISR_Handler242();
extern void x86_64_ISR_Handler243();
extern void x86_64_ISR_Handler244();
extern void x86_64_ISR_Handler245();
extern void x86_64_ISR_Handler246();
extern void x86_64_ISR_Handler247();
extern void x86_64_ISR_Handler248();
extern void x86_64_ISR_Handler249();
extern void x86_64_ISR_Handler250();
extern void x86_64_ISR_Handler251();
extern void x86_64_ISR_Handler252();
extern void x86_64_ISR_Handler253();
extern void x86_64_ISR_Handler254();
extern void x86_64_ISR_Handler255();

}

void x86_64_ISR_SetIDTHandlers() {
    x86_64_IDT_SetHandler(0, x86_64_ISR_Handler0);
    x86_64_IDT_SetHandler(1, x86_64_ISR_Handler1);
    x86_64_IDT_SetHandler(2, x86_64_ISR_Handler2);
    x86_64_IDT_SetHandler(3, x86_64_ISR_Handler3);
    x86_64_IDT_SetHandler(4, x86_64_ISR_Handler4);
    x86_64_IDT_SetHandler(5, x86_64_ISR_Handler5);
    x86_64_IDT_SetHandler(6, x86_64_ISR_Handler6);
    x86_64_IDT_SetHandler(7, x86_64_ISR_Handler7);
    x86_64_IDT_SetHandler(8, x86_64_ISR_Handler8);
    x86_64_IDT_SetHandler(9, x86_64_ISR_Handler9);
    x86_64_IDT_SetHandler(10, x86_64_ISR_Handler10);
    x86_64_IDT_SetHandler(11, x86_64_ISR_Handler11);
    x86_64_IDT_SetHandler(12, x86_64_ISR_Handler12);
    x86_64_IDT_SetHandler(13, x86_64_ISR_Handler13);
    x86_64_IDT_SetHandler(14, x86_64_ISR_Handler14);
    x86_64_IDT_SetHandler(15, x86_64_ISR_Handler15);
    x86_64_IDT_SetHandler(16, x86_64_ISR_Handler16);
    x86_64_IDT_SetHandler(17, x86_64_ISR_Handler17);
    x86_64_IDT_SetHandler(18, x86_64_ISR_Handler18);
    x86_64_IDT_SetHandler(19, x86_64_ISR_Handler19);
    x86_64_IDT_SetHandler(20, x86_64_ISR_Handler20);
    x86_64_IDT_SetHandler(21, x86_64_ISR_Handler21);
    x86_64_IDT_SetHandler(22, x86_64_ISR_Handler22);
    x86_64_IDT_SetHandler(23, x86_64_ISR_Handler23);
    x86_64_IDT_SetHandler(24, x86_64_ISR_Handler24);
    x86_64_IDT_SetHandler(25, x86_64_ISR_Handler25);
    x86_64_IDT_SetHandler(26, x86_64_ISR_Handler26);
    x86_64_IDT_SetHandler(27, x86_64_ISR_Handler27);
    x86_64_IDT_SetHandler(28, x86_64_ISR_Handler28);
    x86_64_IDT_SetHandler(29, x86_64_ISR_Handler29);
    x86_64_IDT_SetHandler(30, x86_64_ISR_Handler30);
    x86_64_IDT_SetHandler(31, x86_64_ISR_Handler31);
    x86_64_IDT_SetHandler(32, x86_64_ISR_Handler32);
    x86_64_IDT_SetHandler(33, x86_64_ISR_Handler33);
    x86_64_IDT_SetHandler(34, x86_64_ISR_Handler34);
    x86_64_IDT_SetHandler(35, x86_64_ISR_Handler35);
    x86_64_IDT_SetHandler(36, x86_64_ISR_Handler36);
    x86_64_IDT_SetHandler(37, x86_64_ISR_Handler37);
    x86_64_IDT_SetHandler(38, x86_64_ISR_Handler38);
    x86_64_IDT_SetHandler(39, x86_64_ISR_Handler39);
    x86_64_IDT_SetHandler(40, x86_64_ISR_Handler40);
    x86_64_IDT_SetHandler(41, x86_64_ISR_Handler41);
    x86_64_IDT_SetHandler(42, x86_64_ISR_Handler42);
    x86_64_IDT_SetHandler(43, x86_64_ISR_Handler43);
    x86_64_IDT_SetHandler(44, x86_64_ISR_Handler44);
    x86_64_IDT_SetHandler(45, x86_64_ISR_Handler45);
    x86_64_IDT_SetHandler(46, x86_64_ISR_Handler46);
    x86_64_IDT_SetHandler(47, x86_64_ISR_Handler47);
    x86_64_IDT_SetHandler(48, x86_64_ISR_Handler48);
    x86_64_IDT_SetHandler(49, x86_64_ISR_Handler49);
    x86_64_IDT_SetHandler(50, x86_64_ISR_Handler50);
    x86_64_IDT_SetHandler(51, x86_64_ISR_Handler51);
    x86_64_IDT_SetHandler(52, x86_64_ISR_Handler52);
    x86_64_IDT_SetHandler(53, x86_64_ISR_Handler53);
    x86_64_IDT_SetHandler(54, x86_64_ISR_Handler54);
    x86_64_IDT_SetHandler(55, x86_64_ISR_Handler55);
    x86_64_IDT_SetHandler(56, x86_64_ISR_Handler56);
    x86_64_IDT_SetHandler(57, x86_64_ISR_Handler57);
    x86_64_IDT_SetHandler(58, x86_64_ISR_Handler58);
    x86_64_IDT_SetHandler(59, x86_64_ISR_Handler59);
    x86_64_IDT_SetHandler(60, x86_64_ISR_Handler60);
    x86_64_IDT_SetHandler(61, x86_64_ISR_Handler61);
    x86_64_IDT_SetHandler(62, x86_64_ISR_Handler62);
    x86_64_IDT_SetHandler(63, x86_64_ISR_Handler63);
    x86_64_IDT_SetHandler(64, x86_64_ISR_Handler64);
    x86_64_IDT_SetHandler(65, x86_64_ISR_Handler65);
    x86_64_IDT_SetHandler(66, x86_64_ISR_Handler66);
    x86_64_IDT_SetHandler(67, x86_64_ISR_Handler67);
    x86_64_IDT_SetHandler(68, x86_64_ISR_Handler68);
    x86_64_IDT_SetHandler(69, x86_64_ISR_Handler69);
    x86_64_IDT_SetHandler(70, x86_64_ISR_Handler70);
    x86_64_IDT_SetHandler(71, x86_64_ISR_Handler71);
    x86_64_IDT_SetHandler(72, x86_64_ISR_Handler72);
    x86_64_IDT_SetHandler(73, x86_64_ISR_Handler73);
    x86_64_IDT_SetHandler(74, x86_64_ISR_Handler74);
    x86_64_IDT_SetHandler(75, x86_64_ISR_Handler75);
    x86_64_IDT_SetHandler(76, x86_64_ISR_Handler76);
    x86_64_IDT_SetHandler(77, x86_64_ISR_Handler77);
    x86_64_IDT_SetHandler(78, x86_64_ISR_Handler78);
    x86_64_IDT_SetHandler(79, x86_64_ISR_Handler79);
    x86_64_IDT_SetHandler(80, x86_64_ISR_Handler80);
    x86_64_IDT_SetHandler(81, x86_64_ISR_Handler81);
    x86_64_IDT_SetHandler(82, x86_64_ISR_Handler82);
    x86_64_IDT_SetHandler(83, x86_64_ISR_Handler83);
    x86_64_IDT_SetHandler(84, x86_64_ISR_Handler84);
    x86_64_IDT_SetHandler(85, x86_64_ISR_Handler85);
    x86_64_IDT_SetHandler(86, x86_64_ISR_Handler86);
    x86_64_IDT_SetHandler(87, x86_64_ISR_Handler87);
    x86_64_IDT_SetHandler(88, x86_64_ISR_Handler88);
    x86_64_IDT_SetHandler(89, x86_64_ISR_Handler89);
    x86_64_IDT_SetHandler(90, x86_64_ISR_Handler90);
    x86_64_IDT_SetHandler(91, x86_64_ISR_Handler91);
    x86_64_IDT_SetHandler(92, x86_64_ISR_Handler92);
    x86_64_IDT_SetHandler(93, x86_64_ISR_Handler93);
    x86_64_IDT_SetHandler(94, x86_64_ISR_Handler94);
    x86_64_IDT_SetHandler(95, x86_64_ISR_Handler95);
    x86_64_IDT_SetHandler(96, x86_64_ISR_Handler96);
    x86_64_IDT_SetHandler(97, x86_64_ISR_Handler97);
    x86_64_IDT_SetHandler(98, x86_64_ISR_Handler98);
    x86_64_IDT_SetHandler(99, x86_64_ISR_Handler99);
    x86_64_IDT_SetHandler(100, x86_64_ISR_Handler100);
    x86_64_IDT_SetHandler(101, x86_64_ISR_Handler101);
    x86_64_IDT_SetHandler(102, x86_64_ISR_Handler102);
    x86_64_IDT_SetHandler(103, x86_64_ISR_Handler103);
    x86_64_IDT_SetHandler(104, x86_64_ISR_Handler104);
    x86_64_IDT_SetHandler(105, x86_64_ISR_Handler105);
    x86_64_IDT_SetHandler(106, x86_64_ISR_Handler106);
    x86_64_IDT_SetHandler(107, x86_64_ISR_Handler107);
    x86_64_IDT_SetHandler(108, x86_64_ISR_Handler108);
    x86_64_IDT_SetHandler(109, x86_64_ISR_Handler109);
    x86_64_IDT_SetHandler(110, x86_64_ISR_Handler110);
    x86_64_IDT_SetHandler(111, x86_64_ISR_Handler111);
    x86_64_IDT_SetHandler(112, x86_64_ISR_Handler112);
    x86_64_IDT_SetHandler(113, x86_64_ISR_Handler113);
    x86_64_IDT_SetHandler(114, x86_64_ISR_Handler114);
    x86_64_IDT_SetHandler(115, x86_64_ISR_Handler115);
    x86_64_IDT_SetHandler(116, x86_64_ISR_Handler116);
    x86_64_IDT_SetHandler(117, x86_64_ISR_Handler117);
    x86_64_IDT_SetHandler(118, x86_64_ISR_Handler118);
    x86_64_IDT_SetHandler(119, x86_64_ISR_Handler119);
    x86_64_IDT_SetHandler(120, x86_64_ISR_Handler120);
    x86_64_IDT_SetHandler(121, x86_64_ISR_Handler121);
    x86_64_IDT_SetHandler(122, x86_64_ISR_Handler122);
    x86_64_IDT_SetHandler(123, x86_64_ISR_Handler123);
    x86_64_IDT_SetHandler(124, x86_64_ISR_Handler124);
    x86_64_IDT_SetHandler(125, x86_64_ISR_Handler125);
    x86_64_IDT_SetHandler(126, x86_64_ISR_Handler126);
    x86_64_IDT_SetHandler(127, x86_64_ISR_Handler127);
    x86_64_IDT_SetHandler(128, x86_64_ISR_Handler128);
    x86_64_IDT_SetHandler(129, x86_64_ISR_Handler129);
    x86_64_IDT_SetHandler(130, x86_64_ISR_Handler130);
    x86_64_IDT_SetHandler(131, x86_64_ISR_Handler131);
    x86_64_IDT_SetHandler(132, x86_64_ISR_Handler132);
    x86_64_IDT_SetHandler(133, x86_64_ISR_Handler133);
    x86_64_IDT_SetHandler(134, x86_64_ISR_Handler134);
    x86_64_IDT_SetHandler(135, x86_64_ISR_Handler135);
    x86_64_IDT_SetHandler(136, x86_64_ISR_Handler136);
    x86_64_IDT_SetHandler(137, x86_64_ISR_Handler137);
    x86_64_IDT_SetHandler(138, x86_64_ISR_Handler138);
    x86_64_IDT_SetHandler(139, x86_64_ISR_Handler139);
    x86_64_IDT_SetHandler(140, x86_64_ISR_Handler140);
    x86_64_IDT_SetHandler(141, x86_64_ISR_Handler141);
    x86_64_IDT_SetHandler(142, x86_64_ISR_Handler142);
    x86_64_IDT_SetHandler(143, x86_64_ISR_Handler143);
    x86_64_IDT_SetHandler(144, x86_64_ISR_Handler144);
    x86_64_IDT_SetHandler(145, x86_64_ISR_Handler145);
    x86_64_IDT_SetHandler(146, x86_64_ISR_Handler146);
    x86_64_IDT_SetHandler(147, x86_64_ISR_Handler147);
    x86_64_IDT_SetHandler(148, x86_64_ISR_Handler148);
    x86_64_IDT_SetHandler(149, x86_64_ISR_Handler149);
    x86_64_IDT_SetHandler(150, x86_64_ISR_Handler150);
    x86_64_IDT_SetHandler(151, x86_64_ISR_Handler151);
    x86_64_IDT_SetHandler(152, x86_64_ISR_Handler152);
    x86_64_IDT_SetHandler(153, x86_64_ISR_Handler153);
    x86_64_IDT_SetHandler(154, x86_64_ISR_Handler154);
    x86_64_IDT_SetHandler(155, x86_64_ISR_Handler155);
    x86_64_IDT_SetHandler(156, x86_64_ISR_Handler156);
    x86_64_IDT_SetHandler(157, x86_64_ISR_Handler157);
    x86_64_IDT_SetHandler(158, x86_64_ISR_Handler158);
    x86_64_IDT_SetHandler(159, x86_64_ISR_Handler159);
    x86_64_IDT_SetHandler(160, x86_64_ISR_Handler160);
    x86_64_IDT_SetHandler(161, x86_64_ISR_Handler161);
    x86_64_IDT_SetHandler(162, x86_64_ISR_Handler162);
    x86_64_IDT_SetHandler(163, x86_64_ISR_Handler163);
    x86_64_IDT_SetHandler(164, x86_64_ISR_Handler164);
    x86_64_IDT_SetHandler(165, x86_64_ISR_Handler165);
    x86_64_IDT_SetHandler(166, x86_64_ISR_Handler166);
    x86_64_IDT_SetHandler(167, x86_64_ISR_Handler167);
    x86_64_IDT_SetHandler(168, x86_64_ISR_Handler168);
    x86_64_IDT_SetHandler(169, x86_64_ISR_Handler169);
    x86_64_IDT_SetHandler(170, x86_64_ISR_Handler170);
    x86_64_IDT_SetHandler(171, x86_64_ISR_Handler171);
    x86_64_IDT_SetHandler(172, x86_64_ISR_Handler172);
    x86_64_IDT_SetHandler(173, x86_64_ISR_Handler173);
    x86_64_IDT_SetHandler(174, x86_64_ISR_Handler174);
    x86_64_IDT_SetHandler(175, x86_64_ISR_Handler175);
    x86_64_IDT_SetHandler(176, x86_64_ISR_Handler176);
    x86_64_IDT_SetHandler(177, x86_64_ISR_Handler177);
    x86_64_IDT_SetHandler(178, x86_64_ISR_Handler178);
    x86_64_IDT_SetHandler(179, x86_64_ISR_Handler179);
    x86_64_IDT_SetHandler(180, x86_64_ISR_Handler180);
    x86_64_IDT_SetHandler(181, x86_64_ISR_Handler181);
    x86_64_IDT_SetHandler(182, x86_64_ISR_Handler182);
    x86_64_IDT_SetHandler(183, x86_64_ISR_Handler183);
    x86_64_IDT_SetHandler(184, x86_64_ISR_Handler184);
    x86_64_IDT_SetHandler(185, x86_64_ISR_Handler185);
    x86_64_IDT_SetHandler(186, x86_64_ISR_Handler186);
    x86_64_IDT_SetHandler(187, x86_64_ISR_Handler187);
    x86_64_IDT_SetHandler(188, x86_64_ISR_Handler188);
    x86_64_IDT_SetHandler(189, x86_64_ISR_Handler189);
    x86_64_IDT_SetHandler(190, x86_64_ISR_Handler190);
    x86_64_IDT_SetHandler(191, x86_64_ISR_Handler191);
    x86_64_IDT_SetHandler(192, x86_64_ISR_Handler192);
    x86_64_IDT_SetHandler(193, x86_64_ISR_Handler193);
    x86_64_IDT_SetHandler(194, x86_64_ISR_Handler194);
    x86_64_IDT_SetHandler(195, x86_64_ISR_Handler195);
    x86_64_IDT_SetHandler(196, x86_64_ISR_Handler196);
    x86_64_IDT_SetHandler(197, x86_64_ISR_Handler197);
    x86_64_IDT_SetHandler(198, x86_64_ISR_Handler198);
    x86_64_IDT_SetHandler(199, x86_64_ISR_Handler199);
    x86_64_IDT_SetHandler(200, x86_64_ISR_Handler200);
    x86_64_IDT_SetHandler(201, x86_64_ISR_Handler201);
    x86_64_IDT_SetHandler(202, x86_64_ISR_Handler202);
    x86_64_IDT_SetHandler(203, x86_64_ISR_Handler203);
    x86_64_IDT_SetHandler(204, x86_64_ISR_Handler204);
    x86_64_IDT_SetHandler(205, x86_64_ISR_Handler205);
    x86_64_IDT_SetHandler(206, x86_64_ISR_Handler206);
    x86_64_IDT_SetHandler(207, x86_64_ISR_Handler207);
    x86_64_IDT_SetHandler(208, x86_64_ISR_Handler208);
    x86_64_IDT_SetHandler(209, x86_64_ISR_Handler209);
    x86_64_IDT_SetHandler(210, x86_64_ISR_Handler210);
    x86_64_IDT_SetHandler(211, x86_64_ISR_Handler211);
    x86_64_IDT_SetHandler(212, x86_64_ISR_Handler212);
    x86_64_IDT_SetHandler(213, x86_64_ISR_Handler213);
    x86_64_IDT_SetHandler(214, x86_64_ISR_Handler214);
    x86_64_IDT_SetHandler(215, x86_64_ISR_Handler215);
    x86_64_IDT_SetHandler(216, x86_64_ISR_Handler216);
    x86_64_IDT_SetHandler(217, x86_64_ISR_Handler217);
    x86_64_IDT_SetHandler(218, x86_64_ISR_Handler218);
    x86_64_IDT_SetHandler(219, x86_64_ISR_Handler219);
    x86_64_IDT_SetHandler(220, x86_64_ISR_Handler220);
    x86_64_IDT_SetHandler(221, x86_64_ISR_Handler221);
    x86_64_IDT_SetHandler(222, x86_64_ISR_Handler222);
    x86_64_IDT_SetHandler(223, x86_64_ISR_Handler223);
    x86_64_IDT_SetHandler(224, x86_64_ISR_Handler224);
    x86_64_IDT_SetHandler(225, x86_64_ISR_Handler225);
    x86_64_IDT_SetHandler(226, x86_64_ISR_Handler226);
    x86_64_IDT_SetHandler(227, x86_64_ISR_Handler227);
    x86_64_IDT_SetHandler(228, x86_64_ISR_Handler228);
    x86_64_IDT_SetHandler(229, x86_64_ISR_Handler229);
    x86_64_IDT_SetHandler(230, x86_64_ISR_Handler230);
    x86_64_IDT_SetHandler(231, x86_64_ISR_Handler231);
    x86_64_IDT_SetHandler(232, x86_64_ISR_Handler232);
    x86_64_IDT_SetHandler(233, x86_64_ISR_Handler233);
    x86_64_IDT_SetHandler(234, x86_64_ISR_Handler234);
    x86_64_IDT_SetHandler(235, x86_64_ISR_Handler235);
    x86_64_IDT_SetHandler(236, x86_64_ISR_Handler236);
    x86_64_IDT_SetHandler(237, x86_64_ISR_Handler237);
    x86_64_IDT_SetHandler(238, x86_64_ISR_Handler238);
    x86_64_IDT_SetHandler(239, x86_64_ISR_Handler239);
    x86_64_IDT_SetHandler(240, x86_64_ISR_Handler240);
    x86_64_IDT_SetHandler(241, x86_64_ISR_Handler241);
    x86_64_IDT_SetHandler(242, x86_64_ISR_Handler242);
    x86_64_IDT_SetHandler(243, x86_64_ISR_Handler243);
    x86_64_IDT_SetHandler(244, x86_64_ISR_Handler244);
    x86_64_IDT_SetHandler(245, x86_64_ISR_Handler245);
    x86_64_IDT_SetHandler(246, x86_64_ISR_Handler246);
    x86_64_IDT_SetHandler(247, x86_64_ISR_Handler247);
    x86_64_IDT_SetHandler(248, x86_64_ISR_Handler248);
    x86_64_IDT_SetHandler(249, x86_64_ISR_Handler249);
    x86_64_IDT_SetHandler(250, x86_64_ISR_Handler250);
    x86_64_IDT_SetHandler(251, x86_64_ISR_Handler251);
    x86_64_IDT_SetHandler(252, x86_64_ISR_Handler252);
    x86_64_IDT_SetHandler(253, x86_64_ISR_Handler253);
    x86_64_IDT_SetHandler(254, x86_64_ISR_Handler254);
    x86_64_IDT_SetHandler(255, x86_64_ISR_Handler255);
}
