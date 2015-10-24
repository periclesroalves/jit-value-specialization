# Just-in-Time Value Specialization in a Nutshell #



## Who we are ##
This project is being developed by researchers and students from the [Programming Language Laboratory](http://www2.dcc.ufmg.br/laboratorios/llp) of the [Federal University of Minas Gerais](http://www.ufmg.br). The working team currently includes Igor Rafael, Péricles Alves, Henrique Santos, and Fernando Pereira. To contact us, please go to the [project members](http://code.google.com/p/jit-value-specialization/people/list) page.

## Our aim ##
Our goal is to develop new [optimizations](https://code.google.com/p/jit-value-specialization/wiki/OptimizationExamples) to execute [JavaScript](http://en.wikipedia.org/wiki/JavaScript) programs efficiently.

## How we plan to do that ##
We plan to speedup JavaScript programs by modifying JavaScript just-in-time compilers to take advantage of an experimental observation: most of the JavaScript functions in actual web pages are called only once during typical browser sessions.

This conclusion comes out of an experiment that we performed: we have instrumented the [Firefox browser](http://en.wikipedia.org/wiki/Firefox), and have used it to navigate through the 100 most visited pages according to the [Alexa Index](http://www.alexa.com). Our conclusion is that almost 50% of the JavaScript functions in these web pages are called only once during a typical browser session. Moreover, almost 60% of them are called with only one set of arguments. That is, even if the same function is called twice or more, it is very likely that it will be called with the same parameters. The chart below illustrates these trends:

<p align='center'><img src='http://wiki.jit-value-specialization.googlecode.com/hg/images/nofcalls.png' width='450' height='220></p'>
<br><b>How many times JavaScript functions are called in the 100 most visited websites.</b>

<p align='center'><img src='http://wiki.jit-value-specialization.googlecode.com/hg/images/difnofcalls.png' width='450' height='220></p'>
<br><b>How many times JavaScript functions are called with different parameters in a typical browser session.</b>

<h2>How we capitalize on this observation</h2>
We have modified a just-in-time JavaScript compiler to specialize the code of a function based on the values of this function's actual parameters. That is, immediately before compiling a function, our JIT compiler inspects the interpreter's stack, to find out the values of the actual parameters of the function. It then uses this knowledge to generate highly specialized code for that function.<br>
<br>
Currently we have implemented our approach on the <a href='https://wiki.mozilla.org/Platform/Features/IonMonkey'>IonMonkey</a> JIT compiler. IonMonkey is Mozilla's newest compiler. We have developed a suite of optimizations that takes advantage of the knowledge of program values. These optimizations are all based on the notion of ''parameter specialization''. By parameter specialization we mean the replacement of actual parameter names by their values. The parameter specialization enables our other optimizations, which we run in the following sequence:<br>
<ul><li><a href='http://en.wikipedia.org/wiki/Inline_function'>Function Inlining</a>.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Loop_inversion'>Loop Inversion</a>.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Constant_propagation'>Constant Propagation</a>.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Dead_code_elimination'>Dead-Code Elimination</a>.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Bounds-checking_elimination'>Array Bounds Check Elimination</a>.<br>
We have also implemented a simple cache, so that, if a function is called again with the same parameters, we can reuse the specialized code.</li></ul>


<h2>What if the function is called with different parameters?</h2>
If the function is called again with different arguments, then we recompiled it, either using a generic approach or doing a new round of specialization.<br>
<br>
Since a just-in-time compiler produces code during the program execution, recompilations may be expensive. Based on the previous observation, we can say that almost 60% of the JS functions in the web would not need a recompilation and, in this case, we have a win-win condition.<br>
<br>
<h2>Is it worth it?</h2>
We have run our specializer on three suites of JavaScript benchmarks, <a href='http://www.webkit.org/perf/sunspider/sunspider.html'>SunSpider 1.0</a>, <a href='http://v8.googlecode.com/svn/data/benchmarks/v6/run.html'>V8 version 6</a>, and <a href='http://krakenbenchmark.mozilla.org'>Kraken 1.1</a>. See the <a href='https://code.google.com/p/jit-value-specialization/wiki/Speedups'>Speedups</a> section information about our speedups in these three benchmarks. We have also measured the overhead of our optimizations on these same benchmarks. Check out the <a href='https://code.google.com/p/jit-value-specialization/wiki/CompilationOverhead'>CompilationOverhead</a> section to see the results.<br>
Our optimization passes also lead to a decrease in the size of the generated machine code. See the <a href='http://code.google.com/p/jit-value-specialization/wiki/CodeSizeReduction'>CodeSizeReduction</a> section for more information.<br>
<br>
<h2>The next steps</h2>

Our specialization shows good results when applied onto numeric benchmarks. However, we can do less with objects, mainly because the Alias Analysis, needed in almost every object based optimization, is still implemented in a very simple way in IonMonkey. Therefore, one of our priorities is to improve the current implementation of Alias Analysis to take advantage of another experimental observation: around 35% of the parameters passed to JavaScript functions that are called with only one set of arguments in the web are objects.<br>
<br>
<p align='center'><img src='http://wiki.jit-value-specialization.googlecode.com/hg/images/types.png' width='450' height='220></p'>
<br><b>Types of the parameters passed to the JavaScript functions that are called with only one set of arguments in the web.</b>

<h1>Reproducing the Experiments</h1>
<h2>Requirements</h2>
In this repository you will find all the files needed to run our implementation and reproduce our experiments. In order to do that, make sure that you have the following programs installed in you machine:<br>
<br>
<ul><li>OS: Unix Linux or Mac OSX.<br>
</li><li>autoconf 2.13.<br>
</li><li>python 2.7 or greater.</li></ul>

<h2>Cloning the repository</h2>
To clone our repository in your machine, use the following command:<br>
<br>
<pre><code>hg clone https://code.google.com/p/jit-value-specialization/<br>
</code></pre>

It will create a folder called <code>jit-value-specialization</code>. Inside that folder you will find the contents of this repository.<br>
<br>
<h2>Building our implementation</h2>
Before running the experiments, you will need to build our implementation. To do that, open the repository folder and use the following commands:<br>
<br>
<pre><code>cd js/src<br>
autoconf2.13<br>
mkdir build_OPT.OBJ<br>
cd build_OPT.OBJ<br>
../configure --enable-optimize<br>
make<br>
</code></pre>

<h2>Running the experiments</h2>
To run the experiments, from the <code>build_OPT.OBJ</code> folder, use the following sequence of commands:<br>
<br>
<pre><code>cd ../../../benchmarks<br>
./run.sh -shell ../js/src/build_OPT.OBJ/js<br>
</code></pre>

The results of the experiments, showing the speedups for various benchmarks, will be presented in the screen.<br>
<br>
<h1>Acknowledgments</h1>

We thank all the Mozilla guys, specially the IonMonkey team. In particular, we thank David Mandelin, David Anderson and Nicolas Pierron.