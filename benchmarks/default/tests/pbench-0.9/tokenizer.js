function tokenizer(s, p) 
{

    l = [];

    var last = 0;

    for (var i = 0; i < s.length; i++) 
    {
        for (var j = 0; j < p.length; j++) 
        {
            if (s[i] == p[j])
            {
                l.push(s.substring(last, i));
                last = i+1;
            }
        }
    }

    return l;
}

tokenizer("IT was a chilly November afternoon. I had just consummated an unusually hearty dinner, of which the dyspeptic truffe formed not the least important item, and was sitting alone in the dining room, with my feet upon the fender, and at my elbow a small table which I had rolled up to the fire, and upon which were some apologies for dessert, with some miscellaneous bottles of wine, spirit and liqueur. In the morning I had been reading Glover s  Leonidas,  Wilkie s  Epigoniad,  Lamartine s  Pilgrimage,  Barlow s  Columbiad,  Tuckermann s  Sicily,  and Griswold s  Curiosities  ; I am willing to confess, therefore, that I now felt a little stupid. I made effort to arouse myself by aid of frequent Lafitte, and, all failing, I betook myself to a stray newspaper in despair. Having carefully perused the column of  houses to let,  and the column of  dogs lost,  and then the two columns of  wives and apprentices runaway,  I attacked with great resolution the editorial matter, and, reading it from beginning to end without understanding a syllable, conceived the possibility of its being Chinese, and so re read it from the end to the beginning, but with no more satisfactory result. I was about throwing away, in disgust, This folio of four pages, happy work Which not even critics criticise, when I felt my attention somewhat aroused by the paragraph which follows :  The avenues to death are numerous and strange. A London paper mentions the decease of a person from a singular cause. He was playing at  puff the dart,  which is played with a long needle inserted in some worsted, and blown at a target through a tin tube. He placed the needle at the wrong end of the tube, and drawing his breath strongly to puff the dart forward with force, drew the needle into his throat. It entered the lungs, and in a few days killed him.  Upon seeing this I fell into a great rage, without exactly knowing why.  This thing,  I exclaimed,  is a contemptible falsehood    a poor hoax    the lees of the invention of some pitiable penny a liner    of some wretched concoctor of accidents in Cocaigne. These fellows, knowing the extravagant gullibility of the age, set their wits to work in the imagination of improbable possibilities    of odd accidents, as they term them; but to a reflecting intellect (like mine,  I added, in parenthesis, putting my forefinger unconsciously to the side of my nose,)  to a contemplative understanding such as I myself possess, it seems evident at once that the marvelous increase of late in these  odd accidents  is by far the oddest accident of all. For my own part, I intend to believe nothing henceforward that has anything of the  singular  about it. IT was a chilly November afternoon. I had just consummated an unusually hearty dinner, of which the dyspeptic truffe formed not the least important item, and was sitting alone in the dining room, with my feet upon the fender, and at my elbow a small table which I had rolled up to the fire, and upon which were some apologies for dessert, with some miscellaneous bottles of wine, spirit and liqueur. In the morning I had been reading Glover s  Leonidas,  Wilkie s  Epigoniad,  Lamartine s  Pilgrimage,  Barlow s  Columbiad,  Tuckermann s  Sicily,  and Griswold s  Curiosities  ; I am willing to confess, therefore, that I now felt a little stupid. I made effort to arouse myself by aid of frequent Lafitte, and, all failing, I betook myself to a stray newspaper in despair. Having carefully perused the column of  houses to let,  and the column of  dogs lost,  and then the two columns of  wives and apprentices runaway,  I attacked with great resolution the editorial matter, and, reading it from beginning to end without understanding a syllable, conceived the possibility of its being Chinese, and so re read it from the end to the beginning, but with no more satisfactory result. I was about throwing away, in disgust, This folio of four pages, happy work Which not even critics criticise, when I felt my attention somewhat aroused by the paragraph which follows :  The avenues to death are numerous and strange. A London paper mentions the decease of a person from a singular cause. He was playing at  puff the dart,  which is played with a long needle inserted in some worsted, and blown at a target through a tin tube. He placed the needle at the wrong end of the tube, and drawing his breath strongly to puff the dart forward with force, drew the needle into his throat. It entered the lungs, and in a few days killed him.  Upon seeing this I fell into a great rage, without exactly knowing why.  This thing,  I exclaimed,  is a contemptible falsehood    a poor hoax    the lees of the invention of some pitiable penny a liner    of some wretched concoctor of accidents in Cocaigne. These fellows, knowing the extravagant gullibility of the age, set their wits to work in the imagination of improbable possibilities    of odd accidents, as they term them; but to a reflecting intellect (like mine,  I added, in parenthesis, putting my forefinger unconsciously to the side of my nose,)  to a contemplative understanding such as I myself possess, it seems evident at once that the marvelous increase of late in these  odd accidents  is by far the oddest accident of all. For my own part, I intend to believe nothing henceforward that has anything of the  singular  about it. IT was a chilly November afternoon. I had just consummated an unusually hearty dinner, of which the dyspeptic truffe formed not the least important item, and was sitting alone in the dining room, with my feet upon the fender, and at my elbow a small table which I had rolled up to the fire, and upon which were some apologies for dessert, with some miscellaneous bottles of wine, spirit and liqueur. In the morning I had been reading Glover s  Leonidas,  Wilkie s  Epigoniad,  Lamartine s  Pilgrimage,  Barlow s  Columbiad,  Tuckermann s  Sicily,  and Griswold s  Curiosities  ; I am willing to confess, therefore, that I now felt a little stupid. I made effort to arouse myself by aid of frequent Lafitte, and, all failing, I betook myself to a stray newspaper in despair. Having carefully perused the column of  houses to let,  and the column of  dogs lost,  and then the two columns of  wives and apprentices runaway,  I attacked with great resolution the editorial matter, and, reading it from beginning to end without understanding a syllable, conceived the possibility of its being Chinese, and so re read it from the end to the beginning, but with no more satisfactory result. I was about throwing away, in disgust, This folio of four pages, happy work Which not even critics criticise, when I felt my attention somewhat aroused by the paragraph which follows :  The avenues to death are numerous and strange. A London paper mentions the decease of a person from a singular cause. He was playing at  puff the dart,  which is played with a long needle inserted in some worsted, and blown at a target through a tin tube. He placed the needle at the wrong end of the tube, and drawing his breath strongly to puff the dart forward with force, drew the needle into his throat. It entered the lungs, and in a few days killed him.  Upon seeing this I fell into a great rage, without exactly knowing why.  This thing,  I exclaimed,  is a contemptible falsehood    a poor hoax    the lees of the invention of some pitiable penny a liner    of some wretched concoctor of accidents in Cocaigne. These fellows, knowing the extravagant gullibility of the age, set their wits to work in the imagination of improbable possibilities    of odd accidents, as they term them; but to a reflecting intellect (like mine,  I added, in parenthesis, putting my forefinger unconsciously to the side of my nose,)  to a contemplative understanding such as I myself possess, it seems evident at once that the marvelous increase of late in these  odd accidents  is by far the oddest accident of all. For my own part, I intend to believe nothing henceforward that has anything of the  singular  about it. IT was a chilly November afternoon. I had just consummated an unusually hearty dinner, of which the dyspeptic truffe formed not the least important item, and was sitting alone in the dining room, with my feet upon the fender, and at my elbow a small table which I had rolled up to the fire, and upon which were some apologies for dessert, with some miscellaneous bottles of wine, spirit and liqueur. In the morning I had been reading Glover s  Leonidas,  Wilkie s  Epigoniad,  Lamartine s  Pilgrimage,  Barlow s  Columbiad,  Tuckermann s  Sicily,  and Griswold s  Curiosities  ; I am willing to confess, therefore, that I now felt a little stupid. I made effort to arouse myself by aid of frequent Lafitte, and, all failing, I betook myself to a stray newspaper in despair. Having carefully perused the column of  houses to let,  and the column of  dogs lost,  and then the two columns of  wives and apprentices runaway,  I attacked with great resolution the editorial matter, and, reading it from beginning to end without understanding a syllable, conceived the possibility of its being Chinese, and so re read it from the end to the beginning, but with no more satisfactory result. I was about throwing away, in disgust, This folio of four pages, happy work Which not even critics criticise, when I felt my attention somewhat aroused by the paragraph which follows :  The avenues to death are numerous and strange. A London paper mentions the decease of a person from a singular cause. He was playing at  puff the dart,  which is played with a long needle inserted in some worsted, and blown at a target through a tin tube. He placed the needle at the wrong end of the tube, and drawing his breath strongly to puff the dart forward with force, drew the needle into his throat. It entered the lungs, and in a few days killed him.  Upon seeing this I fell into a great rage, without exactly knowing why.  This thing,  I exclaimed,  is a contemptible falsehood    a poor hoax    the lees of the invention of some pitiable penny a liner    of some wretched concoctor of accidents in Cocaigne. These fellows, knowing the extravagant gullibility of the age, set their wits to work in the imagination of improbable possibilities    of odd accidents, as they term them; but to a reflecting intellect (like mine,  I added, in parenthesis, putting my forefinger unconsciously to the side of my nose,)  to a contemplative understanding such as I myself possess, it seems evident at once that the marvelous increase of late in these  odd accidents  is by far the oddest accident of all. For my own part, I intend to believe nothing henceforward that has anything of the  singular  about it. IT was a chilly November afternoon. I had just consummated an unusually hearty dinner, of which the dyspeptic truffe formed not the least important item, and was sitting alone in the dining room, with my feet upon the fender, and at my elbow a small table which I had rolled up to the fire, and upon which were some apologies for dessert, with some miscellaneous bottles of wine, spirit and liqueur. In the morning I had been reading Glover s  Leonidas,  Wilkie s  Epigoniad,  Lamartine s  Pilgrimage,  Barlow s  Columbiad,  Tuckermann s  Sicily,  and Griswold s  Curiosities  ; I am willing to confess, therefore, that I now felt a little stupid. I made effort to arouse myself by aid of frequent Lafitte, and, all failing, I betook myself to a stray newspaper in despair. Having carefully perused the column of  houses to let,  and the column of  dogs lost,  and then the two columns of  wives and apprentices runaway,  I attacked with great resolution the editorial matter, and, reading it from beginning to end without understanding a syllable, conceived the possibility of its being Chinese, and so re read it from the end to the beginning, but with no more satisfactory result. I was about throwing away, in disgust, This folio of four pages, happy work Which not even critics criticise, when I felt my attention somewhat aroused by the paragraph which follows :  The avenues to death are numerous and strange. A London paper mentions the decease of a person from a singular cause. He was playing at  puff the dart,  which is played with a long needle inserted in some worsted, and blown at a target through a tin tube. He placed the needle at the wrong end of the tube, and drawing his breath strongly to puff the dart forward with force, drew the needle into his throat. It entered the lungs, and in a few days killed him.  Upon seeing this I fell into a great rage, without exactly knowing why.  This thing,  I exclaimed,  is a contemptible falsehood    a poor hoax    the lees of the invention of some pitiable penny a liner    of some wretched concoctor of accidents in Cocaigne. These fellows, knowing the extravagant gullibility of the age, set their wits to work in the imagination of improbable possibilities    of odd accidents, as they term them; but to a reflecting intellect (like mine,  I added, in parenthesis, putting my forefinger unconsciously to the side of my nose,)  to a contemplative understanding such as I myself possess, it seems evident at once that the marvelous increase of late in these  odd accidents  is by far the oddest accident of all. For my own part, I intend to believe nothing henceforward that has anything of the  singular  about it. ", "accidents"); 
