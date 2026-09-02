(function() {
    'use strict';

    const bar = document.createElement('div');
    bar.id = 'scroll-progress';
    document.body.prepend(bar);

    window.addEventListener('scroll', () => {
        const {
            scrollTop,
            scrollHeight,
            clientHeight
        } = document.documentElement;
        const pct = Math.min(1, scrollTop / (scrollHeight - clientHeight));
        bar.style.transform = `scaleX(${pct})`;
    }, {
        passive: true
    });

    const revealObserver = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.classList.add('is-visible');
                revealObserver.unobserve(entry.target);

            }
        });
    }, {
        threshold: 0.08,
        rootMargin: '0px 0px -40px 0px'
    });

    const heroSection = document.querySelector('main > section:first-child');
    if (heroSection) {
        const appFrame = heroSection.querySelector('.flex-1.w-full');
        const title = heroSection.querySelector('h1');
        const subtitle = heroSection.querySelector('p');
        const cta = heroSection.querySelector('.pt-6');

        if (appFrame) appFrame.classList.add('hero-app-frame');
        if (title) title.classList.add('hero-title');
        if (subtitle) subtitle.classList.add('hero-subtitle');
        if (cta) cta.classList.add('hero-cta');
    }

    const featHeading = document.querySelector('#features .text-center h2');
    if (featHeading) {
        featHeading.classList.add('section-heading', 'reveal');
        revealObserver.observe(featHeading);
    }
    const featSubtitle = document.querySelector('#features .text-center p');
    if (featSubtitle) {
        featSubtitle.classList.add('reveal', 'delay-2');
        revealObserver.observe(featSubtitle);
    }

    document.querySelectorAll('#features .flex.flex-col.lg\\:flex-row').forEach((row, i) => {
        const [imgCol, textCol] = (() => {

            const cols = row.querySelectorAll(':scope > div, :scope > .flex-1');
            return [cols[0], cols[1]];
        })();

        const isReversed = i % 2 !== 0;

        const imgEl = row.querySelector('.feature-img');
        if (imgEl) {
            const wrapper = imgEl.closest('.flex-1') || imgEl.parentElement;
            wrapper.classList.add(isReversed ? 'reveal-right' : 'reveal-left');
            revealObserver.observe(wrapper);
        }

        const textEl = row.querySelector('[style*="flex-direction:column"]');
        if (textEl) {
            textEl.classList.add(isReversed ? 'reveal-left' : 'reveal-right', 'delay-1');
            revealObserver.observe(textEl);
        }
    });

    const bentHeading = document.querySelector('#cpp-perks .text-center h2');
    if (bentHeading) {
        bentHeading.classList.add('section-heading', 'reveal');
        revealObserver.observe(bentHeading);
    }

    document.querySelectorAll('#cpp-perks .glass-tile').forEach((tile, i) => {
        tile.classList.add('reveal-scale', `delay-${(i % 6) + 1}`);
        revealObserver.observe(tile);
    });

    const faqHeading = document.querySelector('#faq h2');
    if (faqHeading) {
        faqHeading.classList.add('reveal-left');
        revealObserver.observe(faqHeading);
    }
    const faqSub = document.querySelector('#faq .faq-label-panel p');
    if (faqSub) {
        faqSub.classList.add('reveal', 'delay-2');
        revealObserver.observe(faqSub);
    }
    const faqLink = document.querySelector('#faq .faq-label-panel a');
    if (faqLink) {
        faqLink.classList.add('reveal', 'delay-3');
        revealObserver.observe(faqLink);
    }

    document.querySelectorAll('.faq-item').forEach((item, i) => {
        item.classList.add('reveal', `delay-${(i % 5) + 1}`);
        revealObserver.observe(item);
    });

    const dlHeading = document.querySelector('#downloads .text-center h2');
    if (dlHeading) {
        dlHeading.classList.add('reveal');
        revealObserver.observe(dlHeading);
    }

    document.querySelectorAll('#downloads .rounded-xl').forEach((card, i) => {
        card.classList.add('reveal-scale', `delay-${i + 1}`);
        revealObserver.observe(card);
    });

    const footerLinks = document.querySelector('footer .flex.justify-center');
    if (footerLinks) {
        footerLinks.classList.add('reveal');
        revealObserver.observe(footerLinks);
    }
    const footerCopy = document.querySelector('footer .text-xs');
    if (footerCopy) {
        footerCopy.classList.add('reveal', 'delay-2');
        revealObserver.observe(footerCopy);
    }

    document.querySelectorAll('nav > a, nav > .flex > a').forEach((link, i) => {
        link.style.cssText += `
      opacity: 0;
      transform: translateY(-8px);
      animation: heroFadeUp 360ms cubic-bezier(0.16,1,0.3,1) ${120 + i * 60}ms both;
    `;
    });

    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function(e) {
            const id = this.getAttribute('href').slice(1);
            const target = id ? document.getElementById(id) : null;
            if (target) {
                e.preventDefault();
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });

    document.querySelectorAll('.feat-badge').forEach(badge => {
        badge.addEventListener('mouseenter', () => {
            badge.style.transition = 'transform 200ms cubic-bezier(0.16,1,0.3,1)';
            badge.style.transform = 'scale(1.06)';
        });
        badge.addEventListener('mouseleave', () => {
            badge.style.transform = 'scale(1)';
        });
    });

    const supportBtn = document.getElementById('supportButtonMain');
    if (supportBtn) {
        window.addEventListener('scroll', () => {
            const scrolled = window.scrollY / (document.body.scrollHeight - window.innerHeight);
            const glow = Math.round(20 + scrolled * 20);
            supportBtn.style.boxShadow = `0 0 ${glow}px rgba(255,0,51,0.4)`;
        }, {
            passive: true
        });
    }

})();