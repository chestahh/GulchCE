/* Diagnostic-only native submission loop. No CRT, game linkage or GL imports.
   Function pointers come from the diagnostic's current GL context. Avoid
   timing Python's per-call dispatch gaps as if they were GPU rendering cost. */
typedef unsigned long long word;
int _fltused;
struct command { void *function; word kind; word a[8]; };
typedef void (__stdcall *f0)(void);
typedef void (__stdcall *f1)(unsigned);
typedef void (__stdcall *f2)(unsigned,unsigned);
typedef void (__stdcall *f3)(unsigned,unsigned,unsigned);
typedef void (__stdcall *f4)(unsigned,unsigned,unsigned,unsigned);
typedef void (__stdcall *f5)(unsigned,unsigned,unsigned,unsigned,unsigned);
typedef void (__stdcall *fp3)(unsigned,unsigned,void *);
typedef void (__stdcall *fp4)(unsigned,unsigned,unsigned,void *);
typedef void (__stdcall *fp6)(unsigned,unsigned,unsigned,unsigned,unsigned,void *);
typedef void (__stdcall *ff4)(float,float,float,float);
typedef void (__stdcall *fd1)(double);

__declspec(dllexport) void __cdecl shadow_replay(struct command const *commands, int count)
{
    int i;
    for(i=0;i<count;i++)
    {
        struct command const *c=commands+i;
        word const *a=c->a;
        switch(c->kind)
        {
        case 0: ((f0)c->function)(); break;
        case 1: ((f1)c->function)((unsigned)a[0]); break;
        case 2: ((f2)c->function)((unsigned)a[0],(unsigned)a[1]); break;
        case 3: ((f3)c->function)((unsigned)a[0],(unsigned)a[1],(unsigned)a[2]); break;
        case 4: ((f4)c->function)((unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(unsigned)a[3]); break;
        case 5: ((f5)c->function)((unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(unsigned)a[3],(unsigned)a[4]); break;
        case 6: ((fp3)c->function)((unsigned)a[0],(unsigned)a[1],(void *)a[2]); break;
        case 7: ((fp4)c->function)((unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(void *)a[3]); break;
        case 8: ((fp6)c->function)((unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(unsigned)a[3],(unsigned)a[4],(void *)a[5]); break;
        case 9:
        {
            union { unsigned bits; float value; } f[4]; int j;
            for(j=0;j<4;j++) f[j].bits=(unsigned)a[j];
            ((ff4)c->function)(f[0].value,f[1].value,f[2].value,f[3].value); break;
        }
        case 10:
        {
            union { word bits; double value; } d; d.bits=a[0];
            ((fd1)c->function)(d.value); break;
        }
        }
    }
}
