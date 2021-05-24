# PL2 Programming Language Infrastructure

I came here to vent my curse! 👿

## Comments from my counterparts

> - It's holy crap!
> - Why do people need shell in modern days?
> - What a mess... totally a disaster of programming!
> - No industrial value found.
> - Why inventing tokenizer over again? Isn't there better choices?
> - ...

So as you can see, you should simply don't use this if you really want to develop something. This library would only give you endless curse, because the library is designed in such a way that as dirty as possible.

## What does PL2 do

PL2 is
  - A library loader that loads a shared object (`libxxx.so`) as a language definition
  - A "shell" parser taking apart commands like `print "Доброы вечер, привет " yourName`, convert them to arrays like `["print", "Доброы вечер, привет ", "yourName"]`
  - A bridge that determines which function to call for one command, pass data to the desired function loaded from shared object, gather return information and determine which command to run next.
  - A toy

PL2 is **not**
  - A programming language
  - A parser generator
  - A library of industrial good

## To start

Too lazy no tutorial. Just read examples.
