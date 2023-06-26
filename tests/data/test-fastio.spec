defserializer rlist-serializer () :

  entry-points :
    byte-list

  deftype byte-list (List<Byte>) : rlist(byte)

  defatom byte (x:Byte) :
    writer: (write-byte(#buffer,x))
    reader: (read-byte(#buffer))
    size: (1)
      
  defcombinator rlist (item:X) (xs:List<X>) :
    writer :
      if empty?(xs) :
        #write[byte](0Y)
      else :
        #write[byte](1Y)
        #write[item](head(xs))
        #write[rlist(item)](tail(xs))
    reader :
      switch(#read[byte]) :
        0Y : List()
        1Y : cons(#read[item], #read[rlist(item)])
        else : #error
    skip :
      switch(#read[byte]) :
        0Y : false
        1Y :
          #skip[item]
          #skip[rlist(item)]
        else :
          #error      
